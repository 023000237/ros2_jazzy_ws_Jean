# led — firmware micro-ROS SOLO LED para ESP32

Proyecto de ESP-IDF 5.4 que conecta la ESP32 al **micro-ROS agent** y enciende
o apaga el LED de a bordo según el topic `/set_led`. Es la **base de visión
artificial**: el nodo de OpenCV de los alumnos actúa sobre el robot sin
necesidad de motores.

Es el equivalente en ESP32 del sketch `sketch/led/led.ino` del Arduino Uno Q.
Para el robot con ruedas usa el proyecto hermano [`../chassis`](../chassis).

| Dirección | Topic | Tipo |
|---|---|---|
| SUB | `/set_led` | `std_msgs/Bool` |
| PUB | `/chassis/heartbeat` | `std_msgs/Int32` (1 Hz, enlace vivo) |

Nodo ROS 2: **`led_esp32`**.

---

## Relación con el proyecto `chassis`

Este proyecto **reutiliza el componente `micro_ros_espidf_component`** del
proyecto `chassis` (vía `EXTRA_COMPONENT_DIRS` en `CMakeLists.txt`), en lugar de
duplicar el submódulo. Por eso:

- Debes tener inicializado el submódulo (lo hace `install.sh` o
  `git submodule update --init --recursive`).
- Su `app-colcon.meta` es **idéntico** al de `chassis` para que ambos compartan
  el mismo `libmicroros` sin reconstruirlo cada vez que cambias de proyecto.

---

## Compilar y flashear

Igual que el proyecto `chassis` (mira su
[README](../chassis/README.md) para el detalle del transporte y el
troubleshooting):

```bash
robot esp                 # entra al contenedor ESP-IDF 5.4
cd /project/led
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 -b 115200 flash monitor
```

> Usa `-b 115200`: muchos adaptadores USB-serial fallan a 460800.

---

## Conectar con el agente

En la SBC (o tu PC), con la ESP32 conectada por USB. Selecciona la variante
`led` en `robot.ini` (`[platform] firmware = led`) o directamente:

```bash
ros2 launch turtlebot_core mcu.launch.py \
  backend:=microros microros_transport:=serial \
  serial_device:=/dev/ttyUSB0 serial_baudrate:=115200
```

Verificar:

```bash
ros2 node list                      # debe aparecer /led_esp32
ros2 topic echo /chassis/heartbeat  # contador subiendo a 1 Hz
ros2 topic pub --once /set_led std_msgs/Bool "{data: true}"   # 💡
```

---

## Dónde va tu código

No tienes que tocar el firmware: tu nodo de OpenCV publica en `/set_led` y el
LED responde.

```python
self.led_pub = self.create_publisher(Bool, '/set_led', 10)
...
self.led_pub.publish(Bool(data=objeto_detectado))
```

Si quieres otro pin, cámbialo en `menuconfig` (`LED Configuration → CONFIG_LED_GPIO`)
o en `sdkconfig.defaults`, no en el código.
