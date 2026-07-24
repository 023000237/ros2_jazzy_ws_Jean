# chassis — firmware micro-ROS para ESP32 (TurtleBot completo)

Proyecto base de ESP-IDF 5.4 que conecta la ESP32 al **micro-ROS agent** que
levanta `mcu.launch.py` del paquete `turtlebot_core`. Es el **TurtleBot
completo**: LED + ruedas. Ya trae el enlace ROS 2 funcionando; tú le agregas el
control de motores.

> ¿Solo quieres el LED (base de visión artificial, sin motores)? Usa el proyecto
> hermano [`../led`](../led) — es el equivalente en ESP32 del sketch `led.ino`
> del Arduino Uno Q. Aquí, igual que en Arduino, **cada variante es un proyecto
> aparte**, no una flag de compilación.

---

## Interfaz ROS 2

El firmware crea el nodo **`chassis_esp32`** con:

| Dirección | Topic                | Tipo                  |
|-----------|----------------------|-----------------------|
| SUB       | `/set_led`           | `std_msgs/Bool`       |
| SUB       | `/cmd_vel`           | `geometry_msgs/Twist` |
| PUB       | `/chassis/heartbeat` | `std_msgs/Int32` (1 Hz, enlace vivo) |

`/set_led` es compatible con los nodos `control_led` y `toggle_led` que ya
existen en `turtlebot_core`.

Al arrancar, el monitor serie confirma:

```
chassis_esp32 [TurtleBot completo] listo: esperando al micro-ROS agent...
```

Puedes ajustar el pin del LED en `menuconfig` (`Chassis Configuration →
CONFIG_CHASSIS_LED_GPIO`) o en `sdkconfig.defaults`.

---

## ⚠️ El transporte se elige al compilar

`RMW_UXRCE_TRANSPORT` se fija en **tiempo de compilación** (vía
`app-colcon.meta`), **no** en runtime. El firmware y el agente deben coincidir:

| Transporte | `app-colcon.meta` | `sdkconfig` | Agente (`mcu.launch.py`) |
|---|---|---|---|
| **Serial** (default) | `RMW_UXRCE_TRANSPORT=custom` | `CONFIG_MICRO_ROS_ESP_UART_TRANSPORT=y` | `microros_transport:=serial` |
| **UDP / WiFi** | `RMW_UXRCE_TRANSPORT=udp` | `CONFIG_MICRO_ROS_ESP_NETIF_WLAN=y` | `microros_transport:=udp4` |

Este proyecto viene configurado en **serial** porque es lo más simple para
empezar: solo necesitas el cable USB, sin credenciales de WiFi.

---

## Compilar y flashear

Todo se hace dentro del contenedor de ESP-IDF:

```bash
robot esp                 # entra al contenedor (ESP-IDF 5.4)
cd /project/chassis
```

### 1. Fijar el target

```bash
idf.py set-target esp32   # o esp32s3, esp32c3, según tu placa
```

### 2. (Opcional) Ajustar configuración

```bash
idf.py menuconfig
# Chassis Configuration → pin del LED
# micro-ROS Settings → transporte, pines UART, IP del agente (si usas UDP)
```

### 3. Compilar

```bash
idf.py build
```

> La **primera compilación tarda varios minutos**: el componente descarga y
> compila `libmicroros.a` desde fuente.

### 4. Flashear y monitorear

```bash
idf.py -p /dev/ttyUSB0 -b 115200 flash monitor
# salir del monitor: Ctrl + ]
```

> **Usa `-b 115200`.** Por defecto esptool intenta flashear a 460800 y muchos
> adaptadores USB-serial (CH340/CP210x) fallan con
> `Unable to verify flash chip connection (No serial data received.)`. Si además
> ves un aviso de cristal mal detectado (`Detected crystal freq ... Unsupported
> crystal in use?`), es el mismo síntoma: baja el baudrate.

---

## Conectar con el agente

En la SBC (o en tu PC), con la ESP32 conectada por USB:

```bash
ros2 launch turtlebot_core mcu.launch.py \
  backend:=microros \
  firmware:=chassis \
  microros_transport:=serial \
  serial_device:=/dev/ttyUSB0 \
  serial_baudrate:=115200
```

> El `serial_baudrate` debe coincidir con el del firmware (115200, definido en
> `main/esp32_serial_transport.c`).

### Verificar que el enlace vive

```bash
ros2 node list                      # debe aparecer /chassis_esp32
ros2 topic echo /chassis/heartbeat  # contador subiendo a 1 Hz

# Probar el LED
ros2 topic pub --once /set_led std_msgs/Bool "{data: true}"

# Probar la mezcla diferencial (mira el monitor serie con debug activado)
ros2 topic pub --once /cmd_vel geometry_msgs/Twist \
  "{linear: {x: 0.2}, angular: {z: 0.5}}"
```

---

## Dónde va tu código

En `main/chassis.c`, dentro de `cmd_vel_callback()`. Ahí ya está resuelta la
**mezcla diferencial**: convierte (v, ω) a velocidad de cada rueda.

```c
const float v_left  = v - (w * WHEEL_SEPARATION / 2.0f);
const float v_right = v + (w * WHEEL_SEPARATION / 2.0f);

// TODO(alumno): aplicar w_left / w_right a los motores.
```

Ajusta primero las constantes de tu chasis (en `chassis.c`):

```c
#define WHEEL_SEPARATION    0.16f   // distancia entre ruedas [m]
#define WHEEL_RADIUS        0.033f  // radio de rueda [m]
```

### Si agregas más publishers o subscribers

Hay un límite fijado al compilar en `app-colcon.meta`
(`RMW_UXRCE_MAX_PUBLISHERS`, `RMW_UXRCE_MAX_SUBSCRIPTIONS`, ambos en 2). Si los
excedes, la creación de la entidad falla en runtime. Súbelos ahí y recompila
desde cero (`idf.py fullclean`), y acuérdate de aumentar también el número de
handles del executor:

```c
#define EXECUTOR_HANDLES 3   // subs + timers
```

---

## Estructura

```
chassis/
├── CMakeLists.txt              # proyecto ESP-IDF
├── app-colcon.meta             # ⚠️ transporte y límites de micro-ROS
├── sdkconfig.defaults          # config por defecto (serial + LED gpio)
├── main/
│   ├── chassis.c               # ← tu código va aquí
│   ├── Kconfig.projbuild       # pin del LED y opciones de depuración
│   ├── esp32_serial_transport.c/.h   # transporte UART para micro-ROS
│   └── CMakeLists.txt
└── components/
    └── micro_ros_espidf_component/   # submódulo (rama jazzy)
```

> El proyecto `../led` **reutiliza** este mismo `components/` (no duplica el
> submódulo), por eso ambos comparten el mismo `app-colcon.meta`.

---

## Troubleshooting

| Síntoma | Causa probable | Solución |
|---|---|---|
| El agente no detecta la ESP32 | Transporte no coincide | Firmware en serial ↔ agente en `serial`; revisa la tabla de arriba |
| `Failed status on line N` al arrancar | Excediste `RMW_UXRCE_MAX_*` | Súbelos en `app-colcon.meta` y `idf.py fullclean && idf.py build` |
| No aparece `/chassis_esp32` | El agente no arrancó o puerto equivocado | Verifica `serial_device` y que la ESP32 esté en `/dev/ttyUSB0` |
| Errores de include en VSCode | Los headers viven en el contenedor | Es normal en el host; compila con `idf.py build` dentro del contenedor |
| Basura en el monitor serie | UART compartido con el log | Es esperado en `UART_NUM_0`; usa otro UART en `menuconfig` si te estorba |
| `CONFIG_X undeclared` tras editar `Kconfig.projbuild` | El build no regeneró la config | `idf.py reconfigure` y vuelve a compilar |
| `error: multi-line comment` | Un `//` termina en `\` | ESP-IDF compila con `-Werror`; quita la diagonal invertida final |
| `Unable to verify flash chip connection` al flashear | El adaptador no aguanta 460800 baud | Flashea con `-b 115200` |
| El agente corre pero nunca dice `session established` | La ESP32 arrancó antes que el agente, **o** otro proceso abrió `/dev/ttyUSB0` y le cambió el baudrate | Resetea la placa con el agente ya corriendo. No abras el puerto con `monitor`/pyserial mientras el agente lo tiene: le rompes la configuración del tty |
