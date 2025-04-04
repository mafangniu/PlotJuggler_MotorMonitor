/**
 * @file datastream_sample.cpp
 * @author mafangniu
 * @brief PlotJuggler 插件数据流实现文件
 * @version 1.0
 * @date 2025-04-03
 *
 * @details
 * 该文件实现了 DataStreamSample 类，作为 PlotJuggler 的插件，支持通过 UDP 接收电机状态数据并进行可视化。
 * 数据源为裸字节流形式发送的 `InteractiveMotorData` 结构体数组。
 * 插件负责将数据解析为结构化变量，并通过 PlotJuggler 的接口显示在图形界面中。
 *
 * 支持功能包括：
 * - 通过 UDP 端口 4015 实时接收电机数据；
 * - 自动解码每帧数据，并根据字段名称绘图；
 * - 独立 UI 界面显示每个电机的错误状态（支持高亮/着色）；
 * - 自动缓存错误帧数据并支持导出为日志文件（`motor_error_log_*.txt`）；
 *
 * 插件与 PlotJuggler 3.x 兼容，不依赖插件 UI 面板，采用额外窗口进行状态展示。
 *
 * ⚙️ 技术点：
 * - 使用多线程分别处理数据流和 UI 更新；
 * - 使用 Qt 元对象系统在非主线程中更新 UI；
 * - 使用标准库进行错误数据缓存与日志输出；
 * - 所有数据按字段顺序手动提取，保证与发送端结构严格对齐；
 *
 * @note
 * 若更改 `InteractiveMotorData` 结构体字段或顺序，需同时修改：
 * - `extract_fields()` 函数字段提取逻辑；
 * - `field_names` 中字段名称顺序；
 * - UI 和日志导出格式。
 *
 */

#include <QDebug>
#include <thread>
#include <mutex>
#include <chrono>
#include <cmath>
#include "datastream_sample.h"
#include "saveErrorLog.h"

// 用于显示错误类型
#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>

using namespace PJ;

/**
 * @brief DataStreamSample 构造函数
 * @param group_count  数据分组数
 * @param var_count    每个分组包含的变量数
 *
 * 该构造函数负责初始化数据流对象，创建数据存储数组，并在 PlotJuggler 中注册变量名称。
 */
DataStreamSample::DataStreamSample(int group_count, int var_count)
    : _group_count(group_count), _var_count(var_count)
{
  qDebug() << "✅ DataStreamSample 构造函数被调用";
  _running = false;
  qRegisterMetaType<std::vector<std::vector<double>>>("std::vector<std::vector<double>>");
  _data_array = std::vector<std::vector<double>>(_group_count, std::vector<double>(_var_count, 0.0));

  // 注册各电机的各个量
  for (int g = 0; g < _group_count; ++g)
  {
    for (int v = 0; v < _var_count; ++v)
    {
      if (v >= field_names.size())
        continue;
      std::string name = "Motor" + std::to_string(g + 1) + "/" + field_names[v];
      dataMap().addNumeric(name);
      qDebug() << "Registered:" << QString::fromStdString(name);
    }
  }

  // 针对错误类型显示界面加空指针保护
  if (!motor_error_labels_.empty())
  {
    for (int i = 0; i < std::min(_group_count, (int)motor_error_labels_.size()); ++i)
    {
      QLabel *label = motor_error_labels_[i];
      if (!label)
        continue; // ✅ 防止空指针崩溃

      int error_val = static_cast<int>(_data_array[i][9] + 0.5);
      QString error_str = errorToText(error_val);
      label->setText(QString("%1 (%2)").arg(error_str).arg(error_val));
    }
  }
}


/**
 * @brief 允许外部程序设置数据
 * @param data  传入的新数据
 */
void DataStreamSample::setData(const std::vector<std::vector<double>> &data)
{
  // 检查数据大小是否符合预期
  if (data.size() != _group_count || data[0].size() != _var_count)
  {
    qDebug() << "Invalid data size!";
    return;
  }

  // 更新数据存储数组
  _data_array = data;

  qDebug() << "setData() received:";
  for (int g = 0; g < _group_count; ++g)
  {
    qDebug() << "Group" << g + 1 << ":" << data[g];
  }

  // 更新数据并通知监听者
  updateData();
}

/**
 * @brief 启动数据流
 * @return 成功启动返回 true
 *
 * 该函数启动数据更新线程和 UDP 数据监听线程。
 */
bool DataStreamSample::start(QStringList *)
{
  _running = true;

  // 启动数据更新线程,
  _thread = std::thread([this]()
                        { this->loop(); });

  // 启动 UDP 数据监听线程（分离模式）
  std::thread udp_thread([this]()
                         { this->receiveUDPData(); });
  udp_thread.detach(); // 分离线程，不阻塞主线程

  // 启动临时窗口显示电机错误类型
  // (所使用的plotjuggler里没有OptionWidgets,而plotjuggler界面里只能显示数据,不能显示文本,额外单独开一个UI界面显示电机错误类型)
  startUIWindow();
  return true;
}

/**
 * @brief 关闭数据流
 */
void DataStreamSample::shutdown()
{
  _running = false;
  if (_thread.joinable())
  {
    _thread.join();
  }
}

/**
 * @brief 检查数据流是否正在运行
 * @return 运行中返回 true，否则返回 false
 */
bool DataStreamSample::isRunning() const
{
  return _running;
}

/**
 * @brief 析构函数，确保数据流正确关闭
 */
DataStreamSample::~DataStreamSample()
{
  shutdown();
}

/**
 * @brief 更新数据并通知 PlotJuggler
 *
 * 该函数会遍历数据数组，并将其推送到 PlotJuggler 进行可视化。
 */
void DataStreamSample::updateData()
{
  std::lock_guard<std::mutex> lock(mutex());

  auto now = std::chrono::high_resolution_clock::now();
  const double stamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();

  // 将各电机的各个值推送到plotjuggler界面
  for (int g = 0; g < _group_count; ++g)
  {
    for (int v = 0; v < _var_count; ++v)
    {
      if (v >= field_names.size())
        continue;

      std::string name = "Motor" + std::to_string(g + 1) + "/" + field_names[v];
      auto it = dataMap().numeric.find(name);
      if (it != dataMap().numeric.end())
      {
        it->second.pushBack(PlotData::Point(stamp, _data_array[g][v]));
      }
      else
      {
        qDebug() << "Not found in dataMap:" << QString::fromStdString(name);
      }
    }
  }

  // 电机错误类型数据是单独界面显示,在此处进行更新
  if (!motor_error_labels_.empty())
  {
    for (int i = 0; i < std::min(_group_count, (int)motor_error_labels_.size()); ++i)
    {
      int error_val = static_cast<int>(_data_array[i][9] + 0.5); // error字段下标
      QString error_str = errorToText(error_val);
      QString text = QString("%1 (%2)").arg(error_str).arg(error_val);

      // ⬇️ 设置颜色：错误为红色，无错为黑色
      QString color = (error_val == 0) ? "black" : "red";

      // ⬇️ 设置字体颜色样式并更新文本
      QMetaObject::invokeMethod(motor_error_labels_[i], "setStyleSheet", Qt::QueuedConnection,
                                Q_ARG(QString, QString("color: %1;").arg(color)));

      QMetaObject::invokeMethod(motor_error_labels_[i], "setText", Qt::QueuedConnection,
                                Q_ARG(QString, text));
    }
  }

  emit dataReceived();
}

/**
 * @brief 数据流循环
 * 该函数以 50Hz 的频率运行，不断调用 updateData() 更新数据。
 */
void DataStreamSample::loop()
{
  while (_running)
  {
    auto prev = std::chrono::high_resolution_clock::now();
    updateData();
    emit dataReceived();
    std::this_thread::sleep_until(prev + std::chrono::milliseconds(1000)); // 1Hz 运行频率
  }
}

/**
 * @brief 监听 UDP 端口并接收数据
 *
 * 该函数会监听 4015 端口，并接收 UDP 发送的数据，解析后更新数据存储数组。
 */
void DataStreamSample::receiveUDPData()
{
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
  {
    qDebug() << "Error: Unable to create socket!";
    return;
  }

  int reuse = 1;
  setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(4015);
  serverAddr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
  {
    qDebug() << "Error: Unable to bind to port 4015!";
    close(sock);
    return;
  }

  qDebug() << "Listening on UDP port 4015...";

  const int MOTOR_COUNT = 13;
  InteractiveMotorData recv_data[MOTOR_COUNT];
  // const int MOTOR_COUNT = 13;
  // const int struct_size = 72;
  // char recv_buffer[MOTOR_COUNT * struct_size];

  std::string timestamp_str_first = "";  // 用于存储最早出现错误时的时间戳  
  while (_running)
  {
    bool current_frame_has_error = false; 
    ssize_t bytesRead = recvfrom(sock, recv_data, sizeof(recv_data), 0, nullptr, nullptr);
    if (bytesRead != sizeof(recv_data))
    {
      qDebug() << "⚠️ UDP接收字节数不匹配：" << bytesRead << " != " << sizeof(recv_data);
      continue;
    }

    std::vector<std::vector<double>> data(_group_count);

    for (int i = 0; i < _group_count; ++i)
    {
      auto values = extract_fields(recv_data[i]);
      data[i] = std::vector<double>(values.begin(), values.begin() + std::min(_var_count, static_cast<int>(values.size())));
    }

    setData(data);

    // 电机错误时,保存数据(只要有一个电机出现错误,就保存后续所有电机的数据)
    // ✅ 检查是否有 error != 0 bool current_frame_has_error = false;
    for (int i = 0; i < MOTOR_COUNT; ++i)
    {
      if (recv_data[i].error_ != 0)
      {
        current_frame_has_error = true;
        break;
      }
    }

    // ✅ 如果错误已经发生过，则继续缓存
    if (error_triggered_ || current_frame_has_error)
    {
      error_triggered_ = true;
      error_data_buffer_.emplace_back(std::vector<InteractiveMotorData>(recv_data, recv_data + MOTOR_COUNT));  // 电机出错后,将电机数据缓存
    }

    // ✅ 如果错误刚刚发生，导出当前累计的帧
    if (current_frame_has_error)
    {
      // 时间戳作为 frame_idx
      std::string timestamp_str = getCurrentTimestampString();
      if("" == timestamp_str_first){
        timestamp_str_first = timestamp_str;
      }
      for (size_t i = 0; i < error_data_buffer_.size(); ++i)
      {
        printMotorDataToFile(error_data_buffer_[i].data(), MOTOR_COUNT, "motor_error_log_" + timestamp_str_first + ".txt", timestamp_str);
      }
      qDebug() << "✅ 已导出错误日志到 motor_error_log.txt";
      error_data_buffer_.clear(); // 可选：导出后清空
      error_triggered_ = false;   // 或保留 true，根据你是否持续记录而定
    }
  }

  close(sock);
}

/**
 * @brief 数据是按char类型发送InteractiveMotorData结构体,该函数用于提取所有字段,并打印(调试用,后期可取消)
 * @param m 输入的电机数据结构体（已反序列化）
 * @return double 数组,包含了提取所有字段（按顺序）
 * @note 注意：字段提取顺序必须与字段在 sender 中定义一致。
 *       若字段数或顺序变动，需同步更新此函数及字段名列表 field_names。
 */
std::vector<double> extract_fields(const InteractiveMotorData &m)
{
  std::cout << "M=" << m.mode
            << ", Index=" << m.index    // 不显示
            << ", P=" << m.pos_
            << ", V=" << m.vel_
            << ", T=" << m.tau_
            << ", Pd=" << m.pos_des_
            << ", Vd=" << m.vel_des_
            << ", Kp=" << m.kp_
            << ", Kd=" << m.kd_
            << ", FF=" << m.ff_ 
            << ", Error=" << m.error_
            << ", Temperature=" << m.temperature_
            << ", Mos Temperature=" << m.mos_temperature_
            << std::endl;
  return {
      m.mode, // "Mode"
      m.pos_,                      // "Pos"
      m.vel_,                      // "Vel"
      m.tau_,                      // "Torque"
      m.pos_des_,                  // "Pos_des"
      m.vel_des_,                  // "Vel_des"
      m.kp_,                       // "Kp"
      m.kd_,                       // "Kd"
      m.ff_,                        // "FF"
      m.error_,                    // “Error”
      m.temperature_,              // "Temperature"
      m.mos_temperature_           // "Mos Temperature"
  };
}

/**
 * @brief 错误类型解释函数,将接收到的错误类型(int) -> 映射为对应错误类型文本信息
 * @param int error 错误类型ID
 * @return 错误类型(文本形式)
 * @note 注意：错误类型映射关系已定义在error_text_map
 */
QString DataStreamSample::errorToText(int error) const
{
  auto it = error_text_map.find(error);
  return (it != error_text_map.end()) ? it->second : "未知错误";
}

extern "C"
{
  PJ::DataStreamer *createPlugin()
  {
    return new DataStreamSample();
  }
}

/**
 * @brief 我的 PlotJuggler 是 3.x 版本（不带插件 UI 功能）,
 *        过在插件的 start() 函数里创建一个临时窗口来展示 UI
 * @param void
 * @return void
 * @note 注意：设置白底黑字，防止深色主题显示不出内容
 */
void DataStreamSample::startUIWindow()
{
  // 创建一个 QWidget 作为主窗口
  QWidget *widget = new QWidget;
  widget->setWindowTitle("Motor Errors");
  widget->resize(400, 400);
  // ✅ 设置白底黑字，防止深色主题显示不出内容
  widget->setStyleSheet("background-color: white; color: black;");

  // 使用网格布局，将标签按表格形式排列
  QGridLayout *layout = new QGridLayout(widget);

  // 添加表头：Motor 和 Error 两列
  QLabel *header1 = new QLabel("Motor");
  QLabel *header2 = new QLabel("Error");
  layout->addWidget(header1, 0, 0);
  layout->addWidget(header2, 0, 1);

  // 清空错误标签数组，避免旧状态残留
  motor_error_labels_.clear();
  // 为每个电机创建一行显示内容
  for (int i = 0; i < _group_count; ++i)
  {
    QLabel *motor_id = new QLabel(QString("Motor[%1]").arg(i + 1));
    QLabel *error_label = new QLabel("N/A");

    layout->addWidget(motor_id, i + 1, 0);
    layout->addWidget(error_label, i + 1, 1);

    motor_error_labels_.push_back(error_label);
  }
  // 将布局应用到窗口
  widget->setLayout(layout);
  widget->show();

  // 将指针保存到成员变量中（可选，用于后续操作窗口）
  ui_window_ = widget;

}
