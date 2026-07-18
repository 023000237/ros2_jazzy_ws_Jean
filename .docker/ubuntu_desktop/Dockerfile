FROM osrf/ros:jazzy-desktop

ENV DEBIAN_FRONTEND=noninteractive

# Dependencias
RUN apt update && apt install -y \
    # Librerías de ROS2 y Python3
    python3-colcon-common-extensions \
    python3-rosdep \
    python3-pip \
    # Herramientas auxiliares
    git \
    nano \
    tmux \
    iputils-ping \
    # Drivers de sensor rplidar
    ros-jazzy-rplidar-ros \
    ros-jazzy-ros-gz \
    # Drivers de simulador TurtleBot3
    ros-jazzy-xacro \
    ros-jazzy-joint-state-publisher \
    ros-jazzy-robot-state-publisher \
    ros-jazzy-turtlebot3 \
    ros-jazzy-turtlebot3-msgs \
    ros-jazzy-turtlebot3-simulations \
    # Drivers de visión artificial
    python3-opencv \
    ros-jazzy-usb-cam \
    ros-jazzy-image-view \
    ros-jazzy-image-transport \
    ros-jazzy-cv-bridge \
    v4l-utils \
    usbutils \
    ffmpeg \
    && rm -rf /var/lib/apt/lists/*

# Inicialización de rosdep
RUN rosdep init || true
RUN rosdep update

# A partir de aquí usamos bash (necesario para 'source' en los RUN siguientes)
SHELL ["/bin/bash", "-c"]

# Agente micro-ROS (ESP32 con ESP-IDF 5.4)
# Se clona y compila en /uros_ws
RUN mkdir -p /uros_ws/src && \
    git clone -b jazzy https://github.com/micro-ROS/micro_ros_setup.git /uros_ws/src/micro_ros_setup && \
    source /opt/ros/jazzy/setup.bash && \
    apt-get update && \
    cd /uros_ws && \
    rosdep install --from-paths src --ignore-src -y && \
    colcon build && \
    source install/local_setup.bash && \
    ros2 run micro_ros_setup create_agent_ws.sh && \
    ros2 run micro_ros_setup build_agent.sh && \
    rm -rf /var/lib/apt/lists/*

# Workspace
WORKDIR /ros2_ws

# Aseguramos que el entorno ROS siempre esté disponible
RUN echo "source /opt/ros/jazzy/setup.bash" >> /root/.bashrc && \
    echo "[ -f /ros2_ws/install/setup.bash ] && source /ros2_ws/install/setup.bash" >> /root/.bashrc && \
    echo "[ -f /uros_ws/install/local_setup.bash ] && source /uros_ws/install/local_setup.bash" >> /root/.bashrc && \
    echo "export TURTLEBOT3_MODEL=burger" >> /root/.bashrc && \
    echo "export GAZEBO_MODEL_PATH=\$GAZEBO_MODEL_PATH:/opt/ros/jazzy/share/turtlebot3_gazebo/models" >> /root/.bashrc

CMD ["bash"]
