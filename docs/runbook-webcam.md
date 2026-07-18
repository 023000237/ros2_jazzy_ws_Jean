# Runbook · Identificar y configurar tu webcam

Cada webcam puede aparecer con un número de dispositivo distinto
(`/dev/video0`, `/dev/video1`, …) y ese número **cambia** según el orden en que
se conectan los USB. Para tener una ruta estable usamos `/dev/v4l/by-id`, que
identifica la cámara por su hardware y no por el orden de conexión.

Al terminar sabrás qué ruta usar y cómo pasársela al launch de la cámara.

---

## 1. Conectar la webcam e identificarla

Conecta la webcam al equipo (al **host**, no hace falta estar en el contenedor)
y lista los dispositivos por id:

```bash
ls -l /dev/v4l/by-id/
```

Verás algo como:

```
usb-046d_C270_HD_WebCam_XXXXXX-video-index0 -> ../../video0
usb-046d_C270_HD_WebCam_XXXXXX-video-index1 -> ../../video1
```

- El nombre incluye **fabricante y modelo** (aquí `046d` = Logitech, `C270`).
- Fíjate en el que termina en **`-video-index0`**: ese es el stream de video
  principal (los `index1+` suelen ser metadata y no dan imagen).

> Si no aparece `/dev/v4l/by-id/`, instala las utilidades V4L2:
> `sudo apt install -y v4l-utils` (dentro del contenedor ya vienen incluidas).

### Confirmar que ese dispositivo da imagen

Puedes ver los formatos que soporta:

```bash
v4l2-ctl --device=/dev/v4l/by-id/usb-046d_C270_HD_WebCam_XXXXXX-video-index0 --list-formats-ext
```

Anota si soporta `YUYV` o `MJPG` — eso define el parámetro `pixel_format`
(ver sección 3).

---

## 2. Lanzar la cámara con `webcam.launch.py`

El launch está en `turtlebot_core` y publica la imagen (`/image_raw`) más las
transforms necesarias para visualizar en RViz2.

Entra al contenedor y lánzalo pasándole **tu** ruta `by-id`:

```bash
robot dev    # entra al contenedor (si no estás dentro)

ros2 launch turtlebot_core webcam.launch.py \
  video_device:=/dev/v4l/by-id/usb-046d_C270_HD_WebCam_XXXXXX-video-index0
```

> **Copia la ruta exacta** que te salió en el paso 1. Usar la ruta `by-id` (en
> vez de `/dev/video0`) evita que deje de funcionar cuando cambia el número de
> dispositivo.

Para verificar que publica:

```bash
ros2 topic hz /image_raw     # debe rondar ~30 Hz
```

Para visualizarla en RViz2, sigue
`ros2_ws/src/turtlebot_core/rviz/README.md`.

---

## 3. Configurar el parámetro de la webcam

El parámetro que eliges es **`video_device`**. Tienes dos formas de fijarlo.

### Opción A · Por línea de comandos (rápida, para probar)

Sobrescribe el argumento al lanzar, como en la sección 2:

```bash
ros2 launch turtlebot_core webcam.launch.py \
  video_device:=/dev/v4l/by-id/usb-046d_C270_HD_WebCam_XXXXXX-video-index0
```

Otros argumentos disponibles (todos con `arg:=valor`):

| Argumento       | Default                | Para qué sirve |
|-----------------|------------------------|----------------|
| `video_device`  | `/dev/video0`          | Ruta V4L2 de la cámara (usa la `by-id`) |
| `parent_frame`  | `base_link`            | Frame padre en el árbol TF (`laser` si solo usas lidar) |
| `camera_frame`  | `camera_link`          | Frame físico de la cámara |
| `optical_frame` | `camera_optical_frame` | Frame óptico con el que se publica la imagen |

### Opción B · Fijarlo en el launch (permanente)

Si siempre usas la misma cámara, edita el **default** en
`ros2_ws/src/turtlebot_core/launch/webcam.launch.py`:

```python
declare_video_device = DeclareLaunchArgument(
    'video_device',
    default_value='/dev/v4l/by-id/usb-046d_C270_HD_WebCam_XXXXXX-video-index0',
    description='Dispositivo V4L2 de la webcam',
)
```

Así podrás lanzar sin pasar el argumento:

```bash
ros2 launch turtlebot_core webcam.launch.py
```

### Ajustar el formato de imagen (`pixel_format`)

Si la cámara no arranca o da error de formato, ajústalo en el mismo archivo
(nodo `usb_cam_node`, parámetro `pixel_format`) según lo que viste con
`v4l2-ctl --list-formats-ext`:

| La cámara soporta | Usa `pixel_format` |
|-------------------|--------------------|
| `YUYV`            | `yuyv` (default)   |
| `MJPG`            | `mjpeg2rgb`        |

```python
'pixel_format': 'mjpeg2rgb',   # cámbialo si tu cámara lo requiere
```

---

## Checklist final ✅

- [ ] La cámara aparece en `/dev/v4l/by-id/` con su modelo.
- [ ] Identificaste el dispositivo `-video-index0`.
- [ ] `ros2 launch turtlebot_core webcam.launch.py video_device:=<tu-ruta>` publica en `/image_raw`.
- [ ] (Opcional) Fijaste tu ruta como `default_value` en `webcam.launch.py`.

Siguiente paso: visualizar en RViz2 →
`ros2_ws/src/turtlebot_core/rviz/README.md`
