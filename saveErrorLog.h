/**
 * @file saveErrorLog.h
 * @brief 错误电机数据的日志导出工具实现头文件
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

#include <iomanip>
#include <fstream>
#include "datastream_sample.h"
#include <ctime>
#include <sstream>

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
void printMotorDataToFile(const InteractiveMotorData motor_data[], int size, const std::string &filename, const std::string &timestamp_str);

/**
 * @brief 获取当前系统时间戳字符串
 *
 * @return std::string 当前系统本地时间戳，格式为 "yyyy-mm-dd-hh-mm-ss"
 *
 * @note 用于作为日志的帧标识（每帧写入日志时用来区分）
 */
std::string getCurrentTimestampString();