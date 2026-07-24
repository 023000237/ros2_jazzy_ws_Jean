# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Reutiliza el componente micro_ros_espidf_component del proyecto chassis
# (es un submodulo; asi evitamos duplicarlo). Para compartir libmicroros sin
# reconstruirlo, este proyecto usa el MISMO app-colcon.meta que chassis.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../chassis/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(led)
