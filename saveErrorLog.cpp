/**
 * @file saveErrorLog.cpp
 * @brief 错误电机数据的日志导出工具实现
 * @author mafangniu
 * @date 2025-04-03
 *
 * @details
 * 本模块用于在电机运行过程中出现错误时，将每帧的完整电机状态数据导出至日志文件。
 * 支持以下功能：
 * - 将 13 个电机的所有字段值写入指定日志文件；
 * - 自动添加每帧的时间戳（格式：yyyy-mm-dd-hh-mm-ss）；
 * - 追加写入，支持多帧连续记录；
 *
 * 与 PlotJuggler 插件联动，作为后端日志记录工具。
 */

#include "saveErrorLog.h"

/**
 * @brief 导出当前帧的电机数据到日志文件（追加模式）
 *
 * @param motor_data     包含所有电机数据的数组（一般为13个电机）
 * @param size           电机数量（数组长度）
 * @param filename       要写入的日志文件名（如 "motor_error_log_2025-04-03-13-00-12.txt"）
 * @param timestamp_str  当前帧的时间戳字符串（yyyy-mm-dd-hh-mm-ss 形式，用作帧标识）
 *
 * @note 日志每帧前加帧号（时间戳），每个电机分段写入，精度保留小数点后 4 位。
 */
void printMotorDataToFile(const InteractiveMotorData motor_data[], int size, const std::string &filename, const std::string &timestamp_str)
{
    std::ofstream ofs(filename, std::ios::app); // 👈 以追加模式打开
    if (!ofs.is_open())
    {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return;
    }

    ofs << std::fixed << std::setprecision(4);
    ofs << "===== Frame [" << timestamp_str << "] =====\n";

    for (int i = 0; i < size; ++i)
    {
        ofs << "Motor[" << i << "]\n";
        ofs << "  Index       : " << motor_data[i].index << "\n";
        ofs << "  Mode       : " << motor_data[i].mode << "\n";
        ofs << "  Position   : " << motor_data[i].pos_ << " rad\n";
        ofs << "  Velocity   : " << motor_data[i].vel_ << " rad/s\n";
        ofs << "  Torque     : " << motor_data[i].tau_ << " N·m\n";
        ofs << "  Pos_des    : " << motor_data[i].pos_des_ << " rad\n";
        ofs << "  Vel_des    : " << motor_data[i].vel_des_ << " rad/s\n";
        ofs << "  Kp         : " << motor_data[i].kp_ << "\n";
        ofs << "  Kd         : " << motor_data[i].kd_ << "\n";
        ofs << "  Feedforward: " << motor_data[i].ff_ << " N·m\n";
        ofs << "  Error: " << motor_data[i].error_ << " \n";
        ofs << "  Temperature: " << motor_data[i].temperature_ << " \n";
        ofs << "  Mos Temperature: " << motor_data[i].mos_temperature_ << " \n";
        ofs << "------------------------------\n";
       
    }

    ofs << "\n";
    ofs.close();
}

/**
 * @brief 获取当前系统时间戳字符串
 * 
 * @return std::string 当前系统本地时间戳，格式为 "yyyy-mm-dd-hh-mm-ss"
 *
 * @note 用于作为日志的帧标识（每帧写入日志时用来区分）
 */
std::string getCurrentTimestampString()
{
    auto now = std::chrono::system_clock::now();
    std::time_t time_now = std::chrono::system_clock::to_time_t(now);

    std::tm tm_local;
#ifdef _WIN32
    localtime_s(&tm_local, &time_now);
#else
    localtime_r(&time_now, &tm_local);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_local, "%Y-%m-%d-%H-%M-%S");
    return oss.str();
}