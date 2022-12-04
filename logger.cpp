#include"logger.h"
#include<string>

std::string currTime() {
    // 获取当前时间，并规范表示
    char tmp[64];
    time_t ptime;
    time(&ptime);  // time_t time (time_t* timer);
    strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&ptime));
    return tmp;
}

Logger::Logger() {
    // 默认构造函数
    m_target = Terminal;
    m_level = Debug;
    //std::cout << "[WELCOME] " << __FILE__ << " " << currTime() << ": " << "server is running." << std::endl;
}


Logger::Logger(Log_Object target, Log_Level level, const std::string& path) {
    m_target = target;
    m_path = path;
    m_level = level;

    std::string strContent =   currTime() + ": " + "server is running\n";
    if (target != Terminal) {
        m_outfile.open(path, std::ios::out | std::ios::app);   // 打开输出文件
        m_outfile << strContent;
    }
    if (target != File) {
        // 如果日志对象不是仅文件
        //std::cout << strContent;
    }
}

Logger::~Logger() {
    std::string  strContent =  currTime() + ": " + "server is closed\n";
    if (m_outfile.is_open()) {
        m_outfile << strContent;
    }
    m_outfile.flush();
    m_outfile.close();
}

void Logger::DEBUG(const std::string& text) {
    output(text, Debug);
}

void Logger::INFO(const std::string& text) {
    output(text, Info);
}

void Logger::WARNING(const std::string& text) {
    output(text, Warning);
}

void Logger::ERRORS(const std::string& text) {
    output(text, Error);
}

void Logger::output(const std::string &text, Log_Level act_level) {
    std::string prefix;
    if (act_level == Debug) prefix = "[Debug] ";
    else if (act_level == Info) prefix = "[Info] ";
    else if (act_level == Warning) prefix = "[Warning] ";
    else if (act_level == Error) prefix = "[Error] ";
    //else prefix = "";
    //prefix += __FILE__;
    //prefix += " ";
    std::string outputContent = prefix + currTime() + ": " + text + "\n";
    if (m_level <= act_level && m_target != File) {
        // 当前等级设定的等级才会显示在终端，且不能是只文件模式
        std::cout << outputContent;
    }
    if (m_target != Terminal)
        m_outfile << outputContent;

    m_outfile.flush();//刷新缓冲区
}