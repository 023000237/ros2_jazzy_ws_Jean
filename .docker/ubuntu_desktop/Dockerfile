FROM osrf/ros:jazzy-desktop

ENV DEBIAN_FRONTEND=noninteractive

# Core dev tools
RUN apt update && apt install -y \
    python3-colcon-common-extensions \
    python3-rosdep \
    python3-pip \
    git \
    nano \
    tmux \
    iputils-ping \
    && rm -rf /var/lib/apt/lists/*

# Initialize rosdep (safe if already initialized)
RUN rosdep init || true
RUN rosdep update

# Workspace
WORKDIR /ros2_ws

# Auto source ROS
RUN echo "source /opt/ros/jazzy/setup.bash" >> /root/.bashrc
