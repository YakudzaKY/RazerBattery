#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <sstream>
#include <ctime>

class Logger {
public:
    static Logger& Instance();
    void Log(const std::string& level, const std::string& message);

private:
    Logger();
    ~Logger();
    std::ofstream logFile;
    std::mutex logMutex;
};

#define LOG_INFO(msg) { std::ostringstream oss; oss << msg; Logger::Instance().Log("INFO", oss.str()); }
#define LOG_ERROR(msg) { std::ostringstream oss; oss << msg; Logger::Instance().Log("ERROR", oss.str()); }
#define LOG_DEBUG(msg) { std::ostringstream oss; oss << msg; Logger::Instance().Log("DEBUG", oss.str()); }
