cmake_minimum_required(VERSION 3.14)
project(MotorMonitor)

set(CMAKE_CXX_STANDARD 17)

# 如果没有本地安装plotjuggler（源码安装），而是通过sudo apt install ros-${ROS_DISTRO}-plotjuggler安装
# 需要说明plotjuggler的库文件在/opt/ros/${ROS_DISTRO}/lib/下
# 其头文件在/opt/ros/${ROS_DISTRO}/include/下
include_directories(
    /opt/ros/noetic/include/
)
link_directories(
    /opt/ros/noetic/lib/
)

# 查找 Qt5 模块
find_package(Qt5 REQUIRED COMPONENTS Core Widgets Xml)

# 插件源文件
set(SRC
    datastream_sample.cpp
    saveErrorLog.cpp
)

# 构建插件
add_library(mafangniu SHARED ${SRC})

# 启用自动 MOC
set_target_properties(mafangniu PROPERTIES AUTOMOC ON)

# 添加定义
target_compile_definitions(mafangniu PRIVATE QT_PLUGIN)

# 链接 Qt 库和 PlotJuggler
target_link_libraries(mafangniu
    Qt5::Core Qt5::Widgets Qt5::Xml
    plotjuggler_base
)

# 安装插件
install(TARGETS mafangniu DESTINATION ${PJ_PLUGIN_INSTALL_DIRECTORY})
