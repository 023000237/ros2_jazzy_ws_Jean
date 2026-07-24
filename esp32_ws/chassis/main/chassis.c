// ---------------------------------------------------------------------------
// chassis — firmware micro-ROS para la ESP32 (TurtleBot COMPLETO: ruedas + LED)
//
// Se conecta al micro-ROS agent que levanta chassis.launch.py del paquete
// turtlebot_core:
//
//   ros2 launch turtlebot_core chassis.launch.py
//        chassis_backend:=microros microros_transport:=serial
//
//   SUB  /set_led            std_msgs/Bool        LED de a bordo
//   SUB  /cmd_vel            geometry_msgs/Twist  velocidad -> ruedas
//   PUB  /chassis/heartbeat  std_msgs/Int32       contador 1 Hz (enlace vivo)
//
// Si SOLO quieres el LED (base de vision artificial, sin motores), NO uses este
// proyecto: flashea el proyecto hermano  esp32_ws/led  (equivale a led.ino del
// Arduino Uno Q). Aqui, igual que en Arduino, cada variante es un proyecto
// aparte, no una flag de compilacion.
//
// A DIFERENCIA del Arduino Uno Q (que usa un "bridge"), aqui el firmware ES un
// nodo ROS 2 de pleno derecho llamado 'chassis_esp32'. El micro-ROS agent solo
// traduce DDS <-> XRCE-DDS; no hay logica intermedia.
//
// El codigo se divide en DOS BLOQUES; entender la frontera es lo clave:
//
//   [1] COMUNICACION CON EL AGENTE (micro-ROS)
//       Suscriptores, publicadores, executor, tarea y transporte. Es el
//       "contrato" con ROS 2 y casi nunca se toca.
//
//   [2] FUNCIONAMIENTO FISICO DEL ROBOT
//       Lo que actua sobre el hardware: el LED (set_led_callback) y la
//       cinematica diferencial (cmd_vel_callback) que reparte la velocidad a
//       las ruedas. AQUI enganchas TU control de motores.
// ---------------------------------------------------------------------------
// El transporte (serial o UDP) se elige en TIEMPO DE COMPILACION.
// Ver README.md de este proyecto.
// ---------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include <rcl/rcl.h>
#include <rcl/error_handling.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>

#include <std_msgs/msg/bool.h>
#include <std_msgs/msg/int32.h>
#include <geometry_msgs/msg/twist.h>

#include <rmw_microxrcedds_c/config.h>
#include <rmw_microros/rmw_microros.h>

#if defined(CONFIG_MICRO_ROS_ESP_NETIF_WLAN) || defined(CONFIG_MICRO_ROS_ESP_NETIF_ENET)
#include <uros_network_interfaces.h>
#endif

#if defined(CONFIG_MICRO_ROS_ESP_UART_TRANSPORT)
#include "esp32_serial_transport.h"
#endif

// --- Trazas de depuracion ---------------------------------------------------
// OJO: cuando el transporte es UART0, la consola comparte el puerto con
// micro-ROS y cada impresion se inyecta dentro del flujo XRCE-DDS. Por eso
// CONFIG_CHASSIS_DEBUG_PRINT viene apagado por defecto.
#if defined(CONFIG_CHASSIS_DEBUG_PRINT)
#define CHASSIS_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define CHASSIS_LOG(fmt, ...) do { } while (0)
#endif

// --- Macros de chequeo de errores (convencion micro-ROS) --------------------
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if ((temp_rc != RCL_RET_OK)) { printf("Failed status on line %d: %d. Aborting.\n", __LINE__, (int)temp_rc); vTaskDelete(NULL); } }
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if ((temp_rc != RCL_RET_OK)) { printf("Failed status on line %d: %d. Continuing.\n", __LINE__, (int)temp_rc); } }

// ===========================================================================
//  BLOQUE [2] - FUNCIONAMIENTO FISICO: hardware y parametros de TU robot
// ===========================================================================
#define LED_GPIO ((gpio_num_t) CONFIG_CHASSIS_LED_GPIO)

// Parametros del robot con ruedas (ajustalos a tu chasis)
#define WHEEL_SEPARATION    0.16f   // distancia entre ruedas [m]
#define WHEEL_RADIUS        0.033f  // radio de rueda [m]

// ===========================================================================
//  BLOQUE [1] - COMUNICACION CON EL AGENTE: entidades ROS (micro-ROS)
// ===========================================================================
// --- Handles del executor ---------------------------------------------------
//     set_led + cmd_vel + heartbeat timer
#define EXECUTOR_HANDLES 3

// --- Entidades ROS ----------------------------------------------------------
rcl_subscription_t set_led_sub;
rcl_subscription_t cmd_vel_sub;
rcl_publisher_t heartbeat_pub;

std_msgs__msg__Bool set_led_msg;
std_msgs__msg__Int32 heartbeat_msg;
geometry_msgs__msg__Twist cmd_vel_msg;

// ===========================================================================
//  BLOQUE [2] - FUNCIONAMIENTO FISICO: que hace el robot al llegar un mensaje
// ===========================================================================
// Estos callbacks los DISPARA la comunicacion [1] (el executor), pero lo que
// hacen es puramente fisico: mover el LED y las ruedas. Aqui va TU hardware.
// ---------------------------------------------------------------------------
// Callback de /set_led — actuacion minima sobre el robot.
//
// Compatible con los nodos control_led / toggle_led de turtlebot_core, y con
// el nodo de OpenCV de los alumnos (publicar std_msgs/Bool al detectar algo).
// ---------------------------------------------------------------------------
void set_led_callback(const void * msgin)
{
    const std_msgs__msg__Bool * msg = (const std_msgs__msg__Bool *)msgin;

    gpio_set_level(LED_GPIO, msg->data ? 1 : 0);
    CHASSIS_LOG("set_led: %s\n", msg->data ? "ON" : "OFF");
}

// ---------------------------------------------------------------------------
// Callback de /cmd_vel — convierte la velocidad del robot (lineal v, angular w)
// a velocidad de cada rueda usando el modelo diferencial:
//
//   v_left  = v - (w * L / 2)
//   v_right = v + (w * L / 2)
//
// >>> AQUI ENGANCHA TU CODIGO: manda estas velocidades a los motores (PWM,
//     driver H-bridge, etc.).
// ---------------------------------------------------------------------------
void cmd_vel_callback(const void * msgin)
{
    const geometry_msgs__msg__Twist * msg = (const geometry_msgs__msg__Twist *)msgin;

    const float v = (float) msg->linear.x;   // [m/s]
    const float w = (float) msg->angular.z;  // [rad/s]

    const float v_left  = v - (w * WHEEL_SEPARATION / 2.0f);
    const float v_right = v + (w * WHEEL_SEPARATION / 2.0f);

    // Velocidad angular de cada rueda [rad/s] (util para control por encoder)
    const float w_left  = v_left  / WHEEL_RADIUS;
    const float w_right = v_right / WHEEL_RADIUS;

    CHASSIS_LOG("cmd_vel: v=%.3f w=%.3f -> ruedas L=%.3f R=%.3f m/s (%.2f / %.2f rad/s)\n",
                v, w, v_left, v_right, w_left, w_right);

    // TODO(alumno): aplicar w_left / w_right a los motores.
    // Ej.: motor_set_speed(MOTOR_LEFT, w_left); motor_set_speed(MOTOR_RIGHT, w_right);
}

// ===========================================================================
//  BLOQUE [1] - COMUNICACION CON EL AGENTE: heartbeat, tarea y transporte
// ===========================================================================
// ---------------------------------------------------------------------------
// Timer 1 Hz -> publica /chassis/heartbeat para confirmar que el enlace vive.
// ---------------------------------------------------------------------------
void heartbeat_timer_callback(rcl_timer_t * timer, int64_t last_call_time)
{
    (void) last_call_time;

    if (timer != NULL) {
        RCSOFTCHECK(rcl_publish(&heartbeat_pub, &heartbeat_msg, NULL));
        heartbeat_msg.data++;
    }
}

// ---------------------------------------------------------------------------
// Tarea principal de micro-ROS
// ---------------------------------------------------------------------------
void micro_ros_task(void * arg)
{
    (void) arg;

    rcl_allocator_t allocator = rcl_get_default_allocator();
    rclc_support_t support;

    rcl_init_options_t init_options = rcl_get_zero_initialized_init_options();
    RCCHECK(rcl_init_options_init(&init_options, allocator));

#if defined(CONFIG_MICRO_ROS_ESP_NETIF_WLAN) || defined(CONFIG_MICRO_ROS_ESP_NETIF_ENET)
    // Transporte UDP: hay que decirle donde vive el agente (menuconfig)
    rmw_init_options_t * rmw_options = rcl_init_options_get_rmw_init_options(&init_options);
    RCCHECK(rmw_uros_options_set_udp_address(CONFIG_MICRO_ROS_AGENT_IP,
                                             CONFIG_MICRO_ROS_AGENT_PORT,
                                             rmw_options));
#endif

    RCCHECK(rclc_support_init_with_options(&support, 0, NULL, &init_options, &allocator));

    // Nodo
    rcl_node_t node = rcl_get_zero_initialized_node();
    RCCHECK(rclc_node_init_default(&node, "chassis_esp32", "", &support));

    // Suscriptor /set_led
    RCCHECK(rclc_subscription_init_default(
        &set_led_sub,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
        "set_led"));

    // Suscriptor /cmd_vel
    RCCHECK(rclc_subscription_init_default(
        &cmd_vel_sub,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(geometry_msgs, msg, Twist),
        "cmd_vel"));

    // Publicador /chassis/heartbeat
    RCCHECK(rclc_publisher_init_default(
        &heartbeat_pub,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Int32),
        "chassis/heartbeat"));

    // Timer del heartbeat (1 Hz)
    rcl_timer_t timer = rcl_get_zero_initialized_timer();
    RCCHECK(rclc_timer_init_default2(
        &timer,
        &support,
        RCL_MS_TO_NS(1000),
        heartbeat_timer_callback,
        true));

    // Executor
    rclc_executor_t executor = rclc_executor_get_zero_initialized_executor();
    RCCHECK(rclc_executor_init(&executor, &support.context, EXECUTOR_HANDLES, &allocator));
    RCCHECK(rclc_executor_add_subscription(&executor, &set_led_sub, &set_led_msg,
                                           &set_led_callback, ON_NEW_DATA));
    RCCHECK(rclc_executor_add_subscription(&executor, &cmd_vel_sub, &cmd_vel_msg,
                                           &cmd_vel_callback, ON_NEW_DATA));
    RCCHECK(rclc_executor_add_timer(&executor, &timer));

    heartbeat_msg.data = 0;

    CHASSIS_LOG("chassis_esp32 [TurtleBot completo] listo: esperando al micro-ROS agent...\n");

    while (1) {
        rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
        usleep(10000);
    }

    // Liberar recursos (no se alcanza en operacion normal)
    RCCHECK(rcl_subscription_fini(&set_led_sub, &node));
    RCCHECK(rcl_subscription_fini(&cmd_vel_sub, &node));
    RCCHECK(rcl_publisher_fini(&heartbeat_pub, &node));
    RCCHECK(rcl_node_fini(&node));

    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------------
// app_main — configura el transporte y lanza la tarea de micro-ROS
// ---------------------------------------------------------------------------
static size_t uart_port = UART_NUM_0;

void app_main(void)
{
    // LED de a bordo
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);

#if defined(CONFIG_MICRO_ROS_ESP_UART_TRANSPORT)
    // --- Transporte SERIAL (default de chassis.launch.py) ---
    rmw_uros_set_custom_transport(
        true,
        (void *) &uart_port,
        esp32_serial_open,
        esp32_serial_close,
        esp32_serial_write,
        esp32_serial_read);
#elif defined(CONFIG_MICRO_ROS_ESP_NETIF_WLAN) || defined(CONFIG_MICRO_ROS_ESP_NETIF_ENET)
    // --- Transporte UDP (WiFi / Ethernet) ---
    (void) uart_port;
    ESP_ERROR_CHECK(uros_network_interface_initialize());
#else
#error Transporte micro-ROS sin configurar: corre 'idf.py menuconfig' -> micro-ROS Settings
#endif

    xTaskCreate(micro_ros_task,
                "uros_task",
                CONFIG_MICRO_ROS_APP_STACK,
                NULL,
                CONFIG_MICRO_ROS_APP_TASK_PRIO,
                NULL);
}
