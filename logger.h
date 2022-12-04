#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <iostream>
#include <fstream>
#include <time.h>


class Logger {
public:
    enum Log_Level { Debug, Info, Warning, Error };// 日志等级
    enum Log_Object { File, Terminal, File_And_Terminal };// 日志输出目标
public:
    Logger();
    Logger(Log_Object target, Log_Level level, const std::string& path);
    ~Logger();
    
    void DEBUG(const std::string& text);
    void INFO(const std::string& text);
    void WARNING(const std::string& text);
    void ERRORS(const std::string& text);

private:
    std::ofstream m_outfile;    // 将日志输出到文件的流对象
    Log_Object m_target;        // 日志输出目标
    std::string m_path;              // 日志文件路径
    Log_Level m_level;          // 日志等级
    void output(const std::string &text, Log_Level act_level);            // 输出行为
};

#endif//_LOGGER_H_