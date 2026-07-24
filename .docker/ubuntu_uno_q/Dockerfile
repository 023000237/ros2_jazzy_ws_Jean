FROM ros:jazzy-ros-base

ENV DEBIAN_FRONTEND=noninteractive

# Dependencias (solo adquisicion)
RUN apt update && apt install -y \
    # Librerías de ROS2 y Python3
    python3-colcon-common-extensions \
    python3-rosdep \
    python3-msgpack \
    python3-pip \
    # Herramientas auxiliares
    git \
    nano \
    tmux \
    iputils-ping \
    # Driver del sensor rplidar
    ros-jazzy-rplidar-ros \
    # Captura de la webcam (sin OpenCV)
    ros-jazzy-usb-cam \
    ros-jazzy-image-transport \
    v4l-utils \
    usbutils \
    ffmpeg \
    && rm -rf /var/lib/apt/lists/*

# Inicialización de rosdep
RUN rosdep init || true
RUN rosdep update

# A partir de aquí usamos bash (necesario para 'source' en los RUN siguientes)
SHELL ["/bin/bash", "-c"]

# Workspace
WORKDIR /ros2_ws

# Aseguramos que el entorno ROS siempre esté disponible
RUN echo "source /opt/ros/jazzy/setup.bash" >> /root/.bashrc && \
    echo "[ -f /ros2_ws/install/setup.bash ] && source /ros2_ws/install/setup.bash" >> /root/.bashrc

CMD ["bash"]
