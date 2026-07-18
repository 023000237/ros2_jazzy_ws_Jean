# Runbook · Instalación del entorno

Guía paso a paso para dejar el workspace listo para desarrollar. Al terminar
tendrás el contenedor de ROS2 Jazzy corriendo, el workspace compilado y el
comando `robot` disponible desde cualquier terminal.

---

## Prerrequisitos

Antes de empezar necesitas:

1. **Docker instalado en Ubuntu.**
   Sigue la [guía oficial de instalación en Ubuntu](https://docs.docker.com/engine/install/ubuntu/). Después, para poder usar `docker` sin `sudo`, agrega tu usuario al grupo:

   ```bash
   sudo usermod -aG docker $USER
   newgrp docker          # o cierra sesión y vuelve a entrar
   docker run hello-world # verifica que funciona
   ```

2. **Tu webcam identificada.**
   Necesitas saber la ruta estable de tu cámara en `/dev/v4l/by-id`.

   → Ver el runbook [`runbook-webcam.md`](./runbook-webcam.md).

3. **Cuenta de GitHub** (para hacer el fork) y **git** instalado:

   ```bash
   sudo apt update && sudo apt install -y git
   ```

---

## 1. Hacer fork del repositorio

1. Entra al [repositorio original](https://github.com/chucholoport/ros2_jazzy_ws) con tu cuenta.
2. Clic en **Fork** (arriba a la derecha) → **Create fork**.
   Esto crea una copia en tu cuenta: `https://github.com/TU_USUARIO/ros2_jazzy_ws`.

---

## 2. Clonar tu fork

> El repo usa **submódulos** (p. ej. `turtlebot_core`). Los inicializamos en el
> paso 4; por eso aquí clonamos normal y luego actualizamos.

```bash
cd ~
git clone https://github.com/TU_USUARIO/ros2_jazzy_ws.git
cd ros2_jazzy_ws
```

---

## 3. Instalar el comando `robot` en tu `.bashrc`

El script `robot` vive en `.docker/scripts/robot` y orquesta el contenedor (`robot dev`, `robot rm`, `robot esp`). Para llamarlo desde cualquier lugar agrega un alias a tu `.bashrc`:

1. Primero, abre el archivo para edición:

   ```bash
   nano ~/.bashrc
   ```

2. Después, en el editor de texto, pega el siguiente contenido:

   ```bash
   # Automatically enable the robot command when entering a robotics project
   robot() {
      if [ -f ".docker/scripts/robot" ]; then
         bash .docker/scripts/robot "$@"
      else
         echo "robot: not inside a robotics project"
      fi
   }
   ```

3. Guarda el archivo con la terminal interactiva debajo del editor:

   - Presiona **`Ctrl` + `O`** (opción *WriteOut* / *Escribir*) para guardar.
   - Nano preguntará el nombre del archivo (`File Name to Write: ~/.bashrc`);
      presiona **`Enter`** para confirmar.
   - Presiona **`Ctrl` + `X`** (opción *Exit* / *Salir*) para cerrar el editor.

   > En la barra inferior de nano el símbolo `^` significa la tecla `Ctrl`
   > (p. ej. `^O` = `Ctrl` + `O`, `^X` = `Ctrl` + `X`).

4. Recarga tu `.bashrc` para aplicar los cambios en la terminal actual:

   ```bash
   source ~/.bashrc
   ```

5. Verifica que quedó registrado:

   ```bash
   robot
   ```

6. Debe imprimir el menú de uso (`dev` / `rm` / `esp`):

   ```bash
   Usage:
      robot dev   → start dev container
      robot rm    → remove container and free space
      robot esp   → start esp container
   ```


> Si clonaste el repo en otra carpeta, ajusta la ruta del alias a esa ubicación.

---

## 4. Abrir el repo en VSCode e inicializar submódulos

1. Abre el repo en VSCode

   ```bash
   code ~/ros2_jazzy_ws
   ```

2. Dentro de la carpeta del repo, baja el código de los submódulos:

   ```bash
   git submodule update --init --recursive
   ```

3. Comprueba que `turtlebot_core` ya tiene contenido:

   ```bash
   ls ros2_ws/src/turtlebot_core
   ```

---

## 5. Levantar el contenedor de desarrollo

```bash
robot dev
```

La **primera vez** construye la imagen (`ubuntu_desktop-ros2`) — clona y compila el agente micro-ROS, descarga los drivers, etc. Puede tardar varios minutos. Las siguientes veces solo arranca el contenedor y te abre una shell dentro.

Al terminar estarás **dentro del contenedor**, en `/ros2_ws`, con el entorno de
ROS2 ya sourced.

---

## 6. Compilar el workspace

Ya dentro del contenedor (`robot dev`):

```bash
cd /ros2_ws
colcon build --symlink-install
source install/setup.bash
```

Verifica que el paquete quedó disponible:

```bash
ros2 pkg list | grep turtlebot_core
```

> El `source` solo aplica a la terminal actual. En cada nueva shell del
> contenedor, la imagen ya sourcea automáticamente `install/setup.bash` si existe
> (está configurado en el `.bashrc` de la imagen), así que normalmente no tienes
> que repetirlo. Si abres una shell justo después de compilar por primera vez,
> ejecuta el `source` una vez.

---

## 7. Desplegar en la SBC (Raspberry) con `robot edge`

La arquitectura reparte el trabajo en dos máquinas:

| Máquina | Comando | Rol |
|---------|---------|-----|
| Tu PC (Ubuntu Desktop) | `robot dev` | Procesamiento pesado (OpenCV) y visualización (RViz2) |
| SBC del robot (Raspberry) | `robot edge` | Solo adquisición: captura la webcam y se conecta a la ESP32 |

En la SBC la instalación es la **misma** (pasos 1–4: fork, clonar, instalar el
comando `robot`, inicializar submódulos), pero en lugar de `robot dev` levantas
el contenedor de edge:

```bash
robot edge
```

La **primera vez** construye la imagen `ubuntu_edge-ros2` (más ligera: sin
simulación, sin OpenCV, sin RViz2) y abre una shell dentro. Ahí compilas igual:

```bash
cd /ros2_ws
colcon build --symlink-install
source install/setup.bash
```

Y lanzas la adquisición (lidar + chassis/ESP32 + webcam):

```bash
ros2 launch turtlebot_core turtlebot_edge.launch.py
```

Como ambas máquinas comparten `ROS_DOMAIN_ID=0` y usan red host, la PC "ve" los
topics de la SBC (`/image_raw`, `/scan`, …) sin configuración extra.

> ### ⚠️ No corras `robot dev` en la SBC
>
> La imagen de `robot dev` incluye el escritorio completo de ROS 2 (RViz2,
> Gazebo, OpenCV, simuladores de TurtleBot3). En una Raspberry **la construcción
> o la ejecución puede agotar la RAM y hacer que el sistema se cuelgue o crashee
> por falta de recursos**. En la SBC usa **siempre `robot edge`**; deja `robot
> dev` para la PC de escritorio.

---

## Instalación completa ✅

Con esto ya tienes:

- El comando `robot` disponible desde tu host.
- El contenedor de ROS2 Jazzy construido y corriendo.
- El workspace compilado y sourced.

### Siguientes pasos

- Identificar y configurar tu webcam → [`runbook-webcam.md`](./runbook-webcam.md)
- Visualizar la cámara en RViz2 → `ros2_ws/src/turtlebot_core/rviz/README.md`

### Comandos útiles del día a día

```bash
robot dev    # PC: contenedor de desarrollo (procesamiento + visualización)
robot edge   # SBC: contenedor de adquisición (webcam + ESP32, sin monitor)
robot rm     # elimina el contenedor para liberar espacio
robot esp    # entra al contenedor de firmware ESP32 / ESP-IDF
```
