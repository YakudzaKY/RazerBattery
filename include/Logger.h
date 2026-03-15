#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <sstream>
#include <ctime>

class Logger {
public:
    static Logger& Instance();
    static bool IsEnabled();
    void Log(const std::string& level, const std::string& message);

private:
    Logger();
    ~Logger();
    std::ofstream logFile;
    std::mutex logMutex;
};

#define LOG_INFO(msg) do { if (Logger::IsEnabled()) { std::ostringstream oss; oss << msg; Logger::Instance().Log("INFO", oss.str()); } } while (0)
#define LOG_ERROR(msg) do { if (Logger::IsEnabled()) { std::ostringstream oss; oss << msg; Logger::Instance().Log("ERROR", oss.str()); } } while (0)
#define LOG_DEBUG(msg) do { if (Logger::IsEnabled()) { std::ostringstream oss; oss << msg; Logger::Instance().Log("DEBUG", oss.str()); } } while (0)
