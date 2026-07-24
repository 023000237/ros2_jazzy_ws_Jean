/**
 * @file    chassis.ino
 * @brief   Firmware del chassis diferencial para Arduino Uno Q.
 *
 * ===========================================================================
 *  EL UNO Q TIENE DOS "CEREBROS" QUE TRABAJAN JUNTOS
 * ===========================================================================
 *   - MPU  (Microprocessor Unit):  corre Linux + ROS 2. Es el ALTO NIVEL:
 *          vision, navegacion y el nodo uno_q_bridge. NO mueve motores.
 *   - MCU  (Microcontroller Unit): corre ESTE sketch. Es el TIEMPO REAL:
 *          lee encoders y genera el PWM de los motores.
 *
 *   Los dos se hablan por el **Arduino RouterBridge** (RPC + notificaciones).
 *   Del lado de ROS 2, quien dialoga con este sketch es `chassis_bridge.py`
 *   del paquete `uno_q_bridge`.
 *
 * ===========================================================================
 *  ESTE ARCHIVO SE DIVIDE EN DOS BLOQUES. Entender la frontera es lo clave:
 * ===========================================================================
 *   [1] COMUNICACION CON EL MPU
 *       Como entran las ordenes desde ROS 2 y como salen los datos de encoder.
 *          - recibe:  set_cmd_vel(vx, wz)       (RPC)    <- viene de /cmd_vel
 *          - envia:   notify("encoder_update")  (evento) -> ticks y velocidades
 *
 *   [2] FUNCIONAMIENTO FISICO DEL ROBOT
 *       Todo lo que toca el hardware real: pines, PWM, encoders y la
 *       cinematica diferencial que reparte la velocidad entre las dos ruedas.
 *
 *   >>> TU personalizas sobre todo el BLOQUE [2] para adaptarlo a TU chasis
 *       (pines, driver de motor, PPR del encoder, medidas). El BLOQUE [1]
 *       casi nunca se toca: es el "contrato" con ROS 2.
 * ===========================================================================
 */

#include "Arduino_RouterBridge.h"


/* =====================================================================
 *  BLOQUE [2] - FUNCIONAMIENTO FISICO: parametros que defines para TU robot
 * ===================================================================== */

/* --- Limites y geometria del robot ------------------------------------ */
static const float MAX_LINEAR  = 0.5f;   /**< Vel. lineal max. [m/s] (= PWM full) */
static const float MAX_ANGULAR = 2.0f;   /**< Vel. angular max. [rad/s] (informativo) */
static const float WHEEL_BASE  = 0.20f;  /**< Distancia entre ruedas [m] */
static const int   MAX_PWM     = 255;    /**< PWM maximo del driver de motor */

/* --- Pines del driver de motores (H-bridge tipo L298 / BTS7960) -------
 *     EN = velocidad (PWM),  IN1/IN2 = sentido de giro                  */
static const int PIN_L_EN  = 5;   /**< Motor IZQUIERDO - PWM (velocidad) */
static const int PIN_L_IN1 = 6;   /**< Motor IZQUIERDO - sentido 1 */
static const int PIN_L_IN2 = 7;   /**< Motor IZQUIERDO - sentido 2 */
static const int PIN_R_EN  = 9;   /**< Motor DERECHO   - PWM (velocidad) */
static const int PIN_R_IN1 = 10;  /**< Motor DERECHO   - sentido 1 */
static const int PIN_R_IN2 = 11;  /**< Motor DERECHO   - sentido 2 */

/* --- Encoders (odometria: cuanto ha girado cada rueda) ---------------- */
static const int   ENCODER_PPR  = 20;     /**< Pulsos por vuelta del encoder */
static const float WHEEL_RADIUS = 0.03f;  /**< Radio de la rueda [m] */
static const int   PIN_ENC_L_A  = 2;      /**< Encoder IZQ canal A (interrupcion INT0) */
static const int   PIN_ENC_L_B  = 4;      /**< Encoder IZQ canal B (sentido de giro) */
static const int   PIN_ENC_R_A  = 3;      /**< Encoder DER canal A (interrupcion INT1) */
static const int   PIN_ENC_R_B  = 12;     /**< Encoder DER canal B (sentido de giro) */

/* --- Estado compartido entre los DOS bloques -------------------------
 *  Este es el "puente" interno entre comunicacion y fisico:
 *   - g_linear / g_angular : las ESCRIBE el bloque [1] (llega una orden ROS)
 *                            y las LEE el bloque [2] (loop, para mover motores).
 *   - g_enc_*              : las escriben las interrupciones de encoder [2]
 *                            y las lee el bloque [1] al reportar a ROS.
 *  Son 'volatile' porque se modifican dentro de interrupciones.
 * --------------------------------------------------------------------- */
static volatile float g_linear   = 0.0f;  /**< Ultima vel. lineal pedida [m/s] */
static volatile float g_angular  = 0.0f;  /**< Ultima vel. angular pedida [rad/s] */
static volatile long  g_enc_left  = 0;    /**< Contador de ticks rueda izquierda */
static volatile long  g_enc_right = 0;    /**< Contador de ticks rueda derecha */

/* --- Prototipos ------------------------------------------------------- */
/* Bloque [2] (fisico): */
static inline int linear_to_pwm(float v);
static void       setMotor(int pwm, int in1, int in2, int en);
static float      ticks_to_velocity(long ticks, float dt);
void              isr_enc_left(void);
void              isr_enc_right(void);
/* Bloque [1] (comunicacion con el MPU): */
void              set_cmd_vel(float vx, float wz);
void              send_encoder_update(long left, long right,
                                      float vel_left, float vel_right);


/* =====================================================================
 *  BLOQUE [1] - COMUNICACION CON EL MPU  (Arduino RouterBridge <-> ROS 2)
 * =====================================================================
 *  Aqui esta TODO el dialogo con Linux/ROS. Dos direcciones:
 *    ENTRA:  set_cmd_vel()         - el MPU nos ordena una velocidad
 *    SALE:   send_encoder_update() - nosotros informamos los encoders
 * ===================================================================== */

/**
 * @brief [ENTRA] Orden de velocidad desde ROS 2 (/cmd_vel -> uno_q_bridge -> RPC).
 *
 * El MPU invoca este metodo por RPC cada vez que hay un /cmd_vel nuevo.
 * NO movemos motores aqui: solo guardamos la orden. El movimiento real lo
 * ejecuta loop() de forma periodica (bloque [2]); asi la comunicacion no se
 * queda bloqueada esperando al hardware.
 *
 * @param vx  Velocidad lineal deseada [m/s].
 * @param wz  Velocidad angular deseada [rad/s].
 */
void set_cmd_vel(float vx, float wz)
{
    g_linear  = vx;
    g_angular = wz;
}

/**
 * @brief [SALE] Envia al MPU los datos de encoder (evento, no bloqueante).
 *
 * Llega a ROS como el mensaje "encoder_update", que chassis_bridge.py convierte
 * en los topics /wheel_ticks y /wheel_velocities.
 *
 * @param left       Ticks acumulados de la rueda izquierda.
 * @param right      Ticks acumulados de la rueda derecha.
 * @param vel_left   Velocidad estimada de la rueda izquierda [m/s].
 * @param vel_right  Velocidad estimada de la rueda derecha [m/s].
 */
void send_encoder_update(long left, long right, float vel_left, float vel_right)
{
    Bridge.notify("encoder_update", left, right, vel_left, vel_right);
}


/* =====================================================================
 *  setup() - arranca los DOS bloques
 * ===================================================================== */
void setup()
{
    /* [2] FISICO: pines de los motores como salida */
    pinMode(PIN_L_EN, OUTPUT);
    pinMode(PIN_L_IN1, OUTPUT);
    pinMode(PIN_L_IN2, OUTPUT);
    pinMode(PIN_R_EN, OUTPUT);
    pinMode(PIN_R_IN1, OUTPUT);
    pinMode(PIN_R_IN2, OUTPUT);

    /* [2] FISICO: pines de encoder como entrada + sus interrupciones.
     *     Cada flanco del canal A dispara la ISR, que mira el canal B
     *     para deducir el sentido de giro. */
    pinMode(PIN_ENC_L_A, INPUT_PULLUP);
    pinMode(PIN_ENC_L_B, INPUT_PULLUP);
    pinMode(PIN_ENC_R_A, INPUT_PULLUP);
    pinMode(PIN_ENC_R_B, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_L_A), isr_enc_left, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_R_A), isr_enc_right, CHANGE);

    /* [1] COMUNICACION: abrir el RouterBridge y registrar el metodo RPC.
     *     provide_safe garantiza que set_cmd_vel se ejecute en el contexto
     *     del loop principal (seguro), no dentro de una interrupcion. */
    Bridge.begin();
    Bridge.provide_safe("set_cmd_vel", set_cmd_vel);
}


/* =====================================================================
 *  loop() - el ciclo de control. Encadena los dos bloques cada ~10 ms:
 *    1) lee la ultima orden recibida      (dato del bloque [1])
 *    2) cinematica + PWM a los motores    (bloque [2])
 *    3) estima y reporta los encoders     (bloque [2] -> bloque [1])
 * ===================================================================== */
void loop()
{
    /* --- Base de tiempo: dt = segundos desde la iteracion anterior --- */
    static unsigned long prev_time = 0;
    unsigned long now = millis();

    if (prev_time == 0) {          // primera vuelta: aun no hay dt valido
        prev_time = now;
        return;
    }

    float dt = (now - prev_time) / 1000.0f;
    prev_time = now;

    /* --- (1) Orden vigente: copiamos el estado que dejo el bloque [1] --- */
    float v = g_linear;
    float w = g_angular;

    /* --- (2) CINEMATICA DIFERENCIAL [FISICO] ------------------------
     *  Reparte la velocidad del robot (v, w) entre las dos ruedas:
     *     rueda_izq = v - (w * L/2)
     *     rueda_der = v + (w * L/2)
     *  Girar (w != 0) hace que una rueda vaya mas rapido que la otra. */
    float v_left  = v - (w * WHEEL_BASE * 0.5f);
    float v_right = v + (w * WHEEL_BASE * 0.5f);

    int pwm_left  = linear_to_pwm(v_left);
    int pwm_right = linear_to_pwm(v_right);

    /* --- (3) Lectura ATOMICA de los encoders ------------------------
     *  Los contadores cambian dentro de interrupciones; los copiamos con
     *  las interrupciones desactivadas para no leer un valor a medias. */
    long enc_left, enc_right;
    noInterrupts();
    enc_left  = g_enc_left;
    enc_right = g_enc_right;
    interrupts();

    /* Ticks nuevos desde la ultima vuelta -> velocidad de cada rueda */
    static long prev_enc_left = 0;
    static long prev_enc_right = 0;
    long delta_left  = enc_left  - prev_enc_left;
    long delta_right = enc_right - prev_enc_right;
    prev_enc_left  = enc_left;
    prev_enc_right = enc_right;

    float vel_left  = ticks_to_velocity(delta_left, dt);
    float vel_right = ticks_to_velocity(delta_right, dt);

    /* --- Reporte a ROS [COMUNICACION], limitado a 20 Hz para no saturar --- */
    static unsigned long prev_pub = 0;
    if ((now - prev_pub) >= 50) {   // 50 ms = 20 Hz
        send_encoder_update(enc_left, enc_right, vel_left, vel_right);
        prev_pub = now;
    }

    /* --- Actuacion: aplicar el PWM calculado a cada motor [FISICO] --- */
    setMotor(pwm_left,  PIN_L_IN1, PIN_L_IN2, PIN_L_EN);
    setMotor(pwm_right, PIN_R_IN1, PIN_R_IN2, PIN_R_EN);

    delay(10);   // ~100 Hz de lazo de control
}


/* =====================================================================
 *  BLOQUE [2] - FUNCIONAMIENTO FISICO: funciones de bajo nivel
 * =====================================================================
 *  Aqui es donde MAS vas a tocar si cambias de driver de motor o de encoder.
 * ===================================================================== */

/**
 * @brief Convierte una velocidad lineal [m/s] al PWM (-255..255) del motor.
 *        El signo indica el sentido de giro; se satura a los limites.
 */
static inline int linear_to_pwm(float v)
{
    float norm = (MAX_LINEAR != 0.0f) ? (v / MAX_LINEAR) : 0.0f;

    if (norm >  1.0f) norm =  1.0f;
    if (norm < -1.0f) norm = -1.0f;

    int pwm = (int)(norm * MAX_PWM + (norm >= 0.0f ? 0.5f : -0.5f));

    if (pwm >  MAX_PWM) pwm =  MAX_PWM;
    else if (pwm < -MAX_PWM) pwm = -MAX_PWM;

    return pwm;
}

/**
 * @brief Aplica un PWM con signo a un motor del H-bridge.
 *        pwm > 0 -> adelante, pwm < 0 -> atras, pwm = 0 -> frenado.
 */
static void setMotor(int pwm, int in1, int in2, int en)
{
    if (pwm >  MAX_PWM) pwm =  MAX_PWM;          // saturacion defensiva
    else if (pwm < -MAX_PWM) pwm = -MAX_PWM;

    if (pwm > 0) {                 // adelante
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
        analogWrite(en, pwm);
    } else if (pwm < 0) {          // atras
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
        analogWrite(en, -pwm);
    } else {                       // parado
        digitalWrite(in1, LOW);
        digitalWrite(in2, LOW);
        analogWrite(en, 0);
    }
}

/**
 * @brief ISR del encoder IZQUIERDO. Salta en cada flanco del canal A;
 *        el canal B indica el sentido (suma o resta un tick).
 */
void isr_enc_left(void)
{
    int b = digitalRead(PIN_ENC_L_B);
    if (b == HIGH) g_enc_left++;
    else           g_enc_left--;
}

/**
 * @brief ISR del encoder DERECHO (misma logica que la izquierda).
 */
void isr_enc_right(void)
{
    int b = digitalRead(PIN_ENC_R_B);
    if (b == HIGH) g_enc_right++;
    else           g_enc_right--;
}

/**
 * @brief Convierte un numero de ticks en la velocidad lineal de la rueda [m/s].
 *        vueltas = ticks/PPR;  distancia = vueltas * 2*pi*r;  vel = distancia/dt.
 */
static float ticks_to_velocity(long ticks, float dt)
{
    if (dt <= 0.0f) return 0.0f;

    float rev  = (float)ticks / ENCODER_PPR;
    float dist = rev * 2.0f * 3.1415926f * WHEEL_RADIUS;
    return dist / dt;
}
