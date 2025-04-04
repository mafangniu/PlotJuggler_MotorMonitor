/**
 * @file datastream_sample.h
 * @author mafangniu
 * @brief PlotJuggler 插件数据流类头文件（UDP实时接收电机数据,并在plotjuggler中显示）
 * @version 1.0
 * @date 2025-04-03
 *
 * @details
 * 该头文件定义了 DataStreamSample 类，用于接收并可视化 UDP 传输的电机状态数据。
 * 插件通过继承 PlotJuggler 的 PJ::DataStreamer 接口，实现：
 *
 * - 启动/停止数据流线程；
 * - 接收按结构体发送的电机数据（InteractiveMotorData）；
 * - 解码后展示在 PlotJuggler 实时曲线图中；
 * - 同时显示每个电机当前的错误类型（文本）,通过单独的Qt界面显示；
 * - 支持 UI 控件界面显示和日志导出。
 *
 * 数据结构为按字节流发送的 struct，其中包含位置、速度、电流等物理量，以及错误码和温度信息。
 * 插件自动解析并绘图，同时提供图形界面用于错误提示与导出。
 *
 * ✅ 支持的功能：
 *   - UDP 数据监听
 *   - 数据解码与更新
 *   - 变量注册与绘图
 *   - 错误码解释与 UI 显示
 *   - 错误日志保存
 *
 * @note 使用该插件需搭配发送端使用同样的数据结构发送 UDP 字节流。
 *
 */

#pragma once

#include <QtPlugin>
#include <thread>
#include <vector>
#include "PlotJuggler/datastreamer_base.h"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdint> // ✅ 必须有这个头文件

// 显示错误类型界面
#include <QString>
#include <QLabel>


// 电机信息结构体
typedef struct
{
  double mode, index;                            // 这里必须是double才行，否则和python那边发过来的对不上，就无法正常通信，电机控制模式，可以起到软急停的作用，模式为0代表停止运动，目前还没实现
  double tau_, pos_, vel_;                       // 当前的位置rad、速度rad/s、力矩N.m
  double pos_des_, vel_des_, kp_, kd_, ff_;      // 期望的位置、速度、比例、积分、力矩N.m
  double error_, temperature_, mos_temperature_; // 读到的错误类型，电机温度和MOS温度
} InteractiveMotorData;
static_assert(sizeof(InteractiveMotorData) == 8 * 13, "Struct size mismatch! Must match sender."); // 已确定发送端为8 * 13字节

std::vector<double> extract_fields(const InteractiveMotorData &m);
// std::vector<double> extract_fields_from_raw(const char *raw_ptr);

// Plotjuggler中显示的var名
// 该量用于注册变量 -> plotjuggler,提取的值和该处字段对应.
static const std::vector<std::string> field_names = {
    "Mode",
    "Pos",
    "Vel",
    "Torque",
    "Pos_des",
    "Vel_des",
    "Kp",
    "Kd",
    "FF",
    "Error",
    "Temperatrue",
    "Mos Temperature"};


// error ID到error类型(文本描述)的映射
static const std::map<int, QString>
    error_text_map =
        {
            {0, "无错误"},
            {1, "电机过热"},
            {2, "电机过流"},
            {3, "电机电压过低"},
            {4, "电机编码器错误"},
            {6, "电机刹车电压过高"},
            {7, "DRV驱动错误"}
        };

/**
 * @class DataStreamSample
 * @brief 数据流示例，支持 UDP 数据接收，并通过 PlotJuggler 进行数据可视化,
 *        另外加了一个独立Qt UI界面显示电机错误类型
 *
 * 该类继承自 `PJ::DataStreamer`，用于实时流式处理数据，并支持外部订阅更新的数据。
 * 它包含多线程处理，包括：
 * - 一个数据流线程（loop()）
 * - 一个 UDP 监听线程（receiveUDPData()）
 *
 * 主要功能包括：
 * - 通过 `start()` 方法启动数据流，并监听 UDP 端口接收数据
 * - 通过 `shutdown()` 关闭数据流
 * - 通过 `setData()` 允许外部手动设置数据
 * - 通过 `updateData()` 处理并推送数据到 PlotJuggler
 * 
 */
class DataStreamSample : public PJ::DataStreamer
{
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "facontidavide.PlotJuggler3.DataStreamer")
  Q_INTERFACES(PJ::DataStreamer)

public:
  /**
   * @brief DataStreamSample 构造函数
   * @param group_count 数据组数，每组包含多个变量
   * @param var_count 每组的变量数
   *
   * 该构造函数初始化数据存储数组，并在 PlotJuggler 中注册数据变量名称。
   */
  DataStreamSample(int group_count = 13, int var_count = 12);

  /**
   * @brief 启动数据流
   * @param 参数列表（未使用）
   * @return 启动成功返回 true
   *
   * 该方法会创建两个线程：
   * - 一个线程运行 `loop()`，以 50Hz 频率更新数据
   * - 一个线程运行 `receiveUDPData()`，监听 UDP 端口接收数据
   */
  virtual bool start(QStringList *) override;

  /**
   * @brief 关闭数据流
   *
   * 该方法会安全终止 `loop()` 线程，但不会直接管理 UDP 线程（该线程为分离线程）。
   */
  virtual void shutdown() override;

  /**
   * @brief 检查数据流是否正在运行
   * @return 如果数据流正在运行，返回 true，否则返回 false
   */
  virtual bool isRunning() const override;

  /**
   * @brief 析构函数
   *
   * 确保在对象销毁时调用 `shutdown()` 以正确终止线程。
   */
  virtual ~DataStreamSample() override;

  /**
   * @brief 获取数据流的名称
   * @return 数据流的名称 "Data Streamer"
   *
   * 该方法用于返回当前数据流的名称，通常用于在 PlotJuggler 内部进行标识。
   */
  virtual const char *name() const override
  {
    return "Data Streamer";
  }

  /**
   * @brief 设置数据数组（外部调用）
   * @param newData 传入的新数据，格式为 `std::vector<std::vector<double>>`
   *
   * 该方法允许外部程序手动更新数据，数据更新后会触发 `updateData()` 并推送到 PlotJuggler。
   */
  void setData(const std::vector<std::vector<double>> &newData);

  /**
   * @brief 监听 UDP 数据
   *
   * 该方法会创建一个 UDP socket 监听端口 `5050`，并在 `_running` 为 `true` 时循环接收数据。
   * 收到数据后会调用 `setData()` 进行更新。
   */
  void receiveUDPData();

signals:
  /**
   * @brief 数据更新信号
   * @param data 更新后的数据数组
   *
   * 该信号会在 `updateData()` 之后发出，通知所有连接的 Qt 槽函数。
   */
  void dataUpdated(const std::vector<std::vector<double>> &data);

private:
  /**
   * @brief 数据流循环
   *
   * 该方法在独立线程 `_thread` 中运行，每 20ms（50Hz）执行一次 `updateData()`。
   * 当 `_running` 设为 `false` 时，循环终止。
   */
  void loop();

  std::thread _thread; ///< 运行数据流的线程
  bool _running; ///< 标志数据流是否正在运行
  int _group_count; ///< 记录数据的分组数
  int _var_count;   ///< 记录每组数据的变量数
  std::vector<std::vector<double>> _data_array; ///< 存储数据数组，每组 `group_count` 个数据，每个包含 `var_count` 个变量

  /**
   * @brief 更新数据并通知订阅者
   *
   * 该方法遍历 `_data_array`，并将数据推送到 PlotJuggler 进行可视化。
   * 每次更新数据后都会发送 `dataUpdated` 信号。
   */
  void updateData();

  // 用于显示错误类型
public:
  /**
   * @brief 我的 PlotJuggler 是 3.x 版本（不带插件 UI 功能）,
   *        过在插件的 start() 函数里创建一个临时窗口来展示 UI
   * @param void
   * @return void
   * @note 注意：设置白底黑字，防止深色主题显示不出内容
   */
  void startUIWindow();

  /**
   * @brief 错误类型解释函数,将接收到的错误类型(int) -> 映射为对应错误类型文本信息
   * @param int error 错误类型ID
   * @return 错误类型(文本形式)
   * @note 注意：错误类型映射关系已定义在error_text_map
   */
  QString errorToText(int mode) const;

private:
  QVector<QLabel *> motor_error_labels_; // 用于保存所有电机错误显示标签（QLabel）的容器
  QWidget *ui_window_ = nullptr; // ✅ 用于保存新建的UI窗口指针

  // 用于出现错误时保存电机数据
private:
  std::vector<std::vector<InteractiveMotorData>> error_data_buffer_;   // 出现错误时缓存电机数据
  bool error_triggered_ = false;   // 出现错误标志
};
