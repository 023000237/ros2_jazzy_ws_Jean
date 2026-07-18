FROM espressif/idf:release-v5.4

ENV DEBIAN_FRONTEND=noninteractive

# Herramientas auxiliares
RUN apt-get update && apt-get install -y \
    git \
    nano \
    tmux \
    udev \
    && rm -rf /var/lib/apt/lists/*

# Dependencias de Python del micro_ros_espidf_component, instaladas en el
# entorno virtual de ESP-IDF (necesarias al compilar la lib micro-ROS con idf.py).
SHELL ["/bin/bash", "-c"]
RUN source /opt/esp/idf/export.sh && \
    pip install --no-cache-dir catkin_pkg lark-parser empy colcon-common-extensions

# Proyecto de firmware (se monta como volumen, editable desde vscode)
WORKDIR /project

CMD ["bash"]
