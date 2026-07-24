// ---------------------------------------------------------------------------
// led — firmware micro-ROS MINIMO para la ESP32 (SOLO LED)
//
// Es la "base de vision artificial": el nodo de OpenCV de los alumnos enciende
// o apaga el LED al detectar algo, SIN necesidad de motores. Es el equivalente
// en ESP-IDF 5.4 del sketch led.ino del Arduino Uno Q.
//
// Para el robot con ruedas usa el proyecto hermano  esp32_ws/chassis  (igual
// que en Arduino, cada variante es un proyecto aparte, no una flag).
//
// Se conecta al micro-ROS agent que levanta chassis.launch.py:
//
//   ros2 launch turtlebot_core chassis.launch.py
//        chassis_backend:=microros microros_transport:=serial
//
//   SUB  /set_led            std_msgs/Bool   LED de a bordo
//   PUB  /chassis/heartbeat  std_msgs/Int32  contador 1 Hz (enlace vivo)
//
// El firmware ES un nodo ROS 2 llamado 'led_esp32'. Dos bloques:
//   [1] COMUNICACION CON EL AGENTE (micro-ROS): entidades, executor, transporte.
//   [2] FUNCIONAMIENTO FISICO: enciende/apaga el LED (set_led_callback).
// ---------------------------------------------------------------------------
// El transporte (serial o UDP) se elige en TIEMPO DE COMPILACION.
// Comparte el componente micro_ros_espidf_component con el proyecto chassis.
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

#include <rmw_microxrcedds_c/config.h>
#include <rmw_microros/rmw_microros.h>

#if defined(CONFIG_MICRO_ROS_ESP_NETIF_WLAN) || defined(CONFIG_MICRO_ROS_ESP_NETIF_ENET)
#include <uros_network_interfaces.h>
#endif

#if defined(CONFIG_MICRO_ROS_ESP_UART_TRANSPORT)
#include "esp32_serial_transport.h"
#endif

// --- Trazas de depuracion ---------------------------------------------------
// OJO: con transporte UART0 la consola comparte el puerto con micro-ROS y cada
// impresion ensucia el flujo XRCE-DDS. Por eso CONFIG_LED_DEBUG_PRINT viene
// apagado por defecto.
#if defined(CONFIG_LED_DEBUG_PRINT)
#define LED_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
#define LED_LOG(fmt, ...) do { } while (0)
#endif

// --- Macros de chequeo de errores (convencion micro-ROS) --------------------
#define RCCHECK(fn) { rcl_ret_t temp_rc = fn; if ((temp_rc != RCL_RET_OK)) { printf("Failed status on line %d: %d. Aborting.\n", __LINE__, (int)temp_rc); vTaskDelete(NULL); } }
#define RCSOFTCHECK(fn) { rcl_ret_t temp_rc = fn; if ((temp_rc != RCL_RET_OK)) { printf("Failed status on line %d: %d. Continuing.\n", __LINE__, (int)temp_rc); } }

// ===========================================================================
//  BLOQUE [2] - FUNCIONAMIENTO FISICO: hardware
// ===========================================================================
#define LED_GPIO ((gpio_num_t) CONFIG_LED_GPIO)

// ===========================================================================
//  BLOQUE [1] - COMUNICACION CON EL AGENTE: entidades ROS (micro-ROS)
// ===========================================================================
// --- Handles del executor: set_led + heartbeat timer ---
#define EXECUTOR_HANDLES 2

rcl_subscription_t set_led_sub;
rcl_publisher_t heartbeat_pub;

std_msgs__msg__Bool set_led_msg;
std_msgs__msg__Int32 heartbeat_msg;

// ===========================================================================
//  BLOQUE [2] - FUNCIONAMIENTO FISICO: que hace el robot al llegar un mensaje
// ===========================================================================
// Callback de /set_led — el executor [1] lo dispara; aqui solo actuamos sobre
// el LED. Compatible con control_led / toggle_led y con el nodo de OpenCV de
// los alumnos (publicar std_msgs/Bool al detectar algo).
// ---------------------------------------------------------------------------
void set_led_callback(const void * msgin)
{
    const std_msgs__msg__Bool * msg = (const std_msgs__msg__Bool *)msgin;

    gpio_set_level(LED_GPIO, msg->data ? 1 : 0);
    LED_LOG("set_led: %s\n", msg->data ? "ON" : "OFF");
}

// ===========================================================================
//  BLOQUE [1] - COMUNICACION CON EL AGENTE: heartbeat, tarea y transporte
// ===========================================================================
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
    RCCHECK(rclc_node_init_default(&node, "led_esp32", "", &support));

    // Suscriptor /set_led
    RCCHECK(rclc_subscription_init_default(
        &set_led_sub,
        &node,
        ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, Bool),
        "set_led"));

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
    RCCHECK(rclc_executor_add_timer(&executor, &timer));

    heartbeat_msg.data = 0;

    LED_LOG("led_esp32 [solo LED / vision artificial] listo: esperando al micro-ROS agent...\n");

    while (1) {
        rclc_executor_spin_some(&executor, RCL_MS_TO_NS(100));
        usleep(10000);
    }

    // Liberar recursos (no se alcanza en operacion normal)
    RCCHECK(rcl_subscription_fini(&set_led_sub, &node));
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
