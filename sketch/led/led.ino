/**
 * @file    led.ino
 * @brief   Sketch MINIMO del chassis para Arduino Uno Q: SOLO LED.
 *
 * ===========================================================================
 *  LA "BASE DE VISION ARTIFICIAL"
 * ===========================================================================
 *  El nodo de OpenCV de los alumnos enciende o apaga un LED al detectar algo,
 *  SIN necesidad de motores. Es el punto de partida mas simple.
 *
 *  Equivale al MODO LED del firmware de la ESP32 (chassis.c). La ventaja de
 *  Arduino es que aqui es, sencillamente, OTRO sketch: para el robot con ruedas
 *  abre el sketch hermano  ../chassis/chassis.ino.
 *
 * ===========================================================================
 *  DOS BLOQUES, igual que chassis.ino:
 * ===========================================================================
 *   [1] COMUNICACION CON EL MPU  -> recibe set_led_state(on) por RPC
 *   [2] FUNCIONAMIENTO FISICO    -> enciende/apaga el LED de a bordo
 *
 *  Del lado de ROS 2 lo maneja el nodo `uno_q_bridge` (uno_q_bridge.py), que
 *  traduce  /set_led (std_msgs/Bool)  ->  RPC "set_led_state".
 *  (El sketch completo, en cambio, se empareja con `chassis_bridge`.)
 * ===========================================================================
 */

#include "Arduino_RouterBridge.h"


/* =====================================================================
 *  BLOQUE [2] - FUNCIONAMIENTO FISICO
 * ===================================================================== */

/** @brief Pin del LED de a bordo. Cambialo si conectas el LED a otro pin. */
static const int PIN_LED = LED_BUILTIN;


/* =====================================================================
 *  BLOQUE [1] - COMUNICACION CON EL MPU  (Arduino RouterBridge <-> ROS 2)
 * =====================================================================
 *  El MPU (Linux + ROS 2) invoca este metodo por RPC cada vez que llega un
 *  /set_led nuevo. Lo unico que hacemos es actuar sobre el hardware [2].
 *
 *  @param on  true = encender el LED, false = apagarlo.
 * ===================================================================== */
void set_led_state(bool on)
{
    digitalWrite(PIN_LED, on ? HIGH : LOW);   // [2] actua sobre el LED
}


/* =====================================================================
 *  setup() - arranca los dos bloques
 * ===================================================================== */
void setup()
{
    /* [2] FISICO: LED como salida, apagado al inicio */
    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    /* [1] COMUNICACION: abrir el RouterBridge y registrar el metodo RPC.
     *     provide_safe garantiza que set_led_state se ejecute en el contexto
     *     del loop principal (seguro). */
    Bridge.begin();
    Bridge.provide_safe("set_led_state", set_led_state);
}


/* =====================================================================
 *  loop() - no hay trabajo activo: el LED reacciona a las ordenes del MPU.
 *  El pequeno delay le da tiempo al RouterBridge de despachar los RPC.
 * ===================================================================== */
void loop()
{
    delay(10);
}
