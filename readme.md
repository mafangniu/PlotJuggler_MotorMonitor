# PlotJuggler_MotorMonitor
![image](https://github.com/user-attachments/assets/3da02d95-f799-4b46-95eb-914a096157b9)


## 1.功能说明
    用于监测电机数据（13个电机，每个包含速度/位置/电流/温度/Mos温度/错误类型ID等）通过plotjuggler显示
    由于plotjuggler只能显示double类型,显示不了文本,加了一个独立的Qt界面,显示电机错误类型(文本)
    其中电机数据是通过Socker发送,工程中通过监听指定端口获取数据.
## 2.依赖说明
    该方法只是实现了电机数据的获取,编译成库的形式实现插件数据流类,显示还需要安装plotjuggler
    但PlotJuggler 本身是一个独立的 Qt 应用程序，可以在 非 ROS 环境下使用，所以可以不用源码安装
    可以使用sudo apt install ros-${ROS_DISTRO}-plotjuggler进行安装，再CMakeLists.txt中指明其头文件和库文件所在位置即可
## 3.使用说明
    （1）编译该工程
         mkdir build
         cd build
         cmake ..
         make -j4
    （2）将生成的库文件libmafangniu.so放到plotjuggler可以加载的位置
         plotjuggler界面中app -> appearance -> Plugins -> + 添加 
    （3）运行plotjuggler,Streaming选择 Data Streamer即可,需要可视化哪些量只需要将其拖到右边的窗口就行
    （4）在电机错误类型显示界面可以选择日志记录方式，分为记录完整运行日志和仅记录出错后日志两种，日志保存在/tmp/plotjuggler_motor_monitor_log下，运行程序后会自动生成该文件夹，日志文件以motor_error/full_log_+时间戳命名
   

![image](https://github.com/user-attachments/assets/507547fc-31e5-4bf7-9f2e-5a7613501aca)

