# esp32_ws

Proyecto(s) de firmware ESP-IDF 5.4 para la ESP32 con micro-ROS.

Esta carpeta se monta en `/project` dentro del container `esp32_dev_lab`
(ver `.docker/esp32/`). Se abre con:

```bash
.docker/scripts/esp
```

## Crear el firmware con micro-ROS

Dentro del container:

```bash
# 1. Crear el proyecto ESP-IDF (o clonar el tuyo)
idf.py create-project mi_firmware
cd mi_firmware

# 2. Agregar el componente micro-ROS (rama jazzy) como submodule
git submodule add -b jazzy \
    https://github.com/micro-ROS/micro_ros_espidf_component.git \
    components/micro_ros_espidf_component

# 3. Configurar el target y compilar
idf.py set-target esp32
idf.py build

# 4. Flashear y monitorear (ajusta el puerto)
idf.py -p /dev/ttyUSB0 flash monitor
```

El agente que dialoga con este firmware corre en el container de ROS 2
(`ros2 dev`), por serial o UDP:

```bash
ros2 run micro_ros_agent micro_ros_agent serial --dev /dev/ttyUSB0 -b 115200
# o
ros2 run micro_ros_agent micro_ros_agent udp4 --port 8888
```
