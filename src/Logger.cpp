#include "Logger.h"
#include <iostream>
#include <filesystem>
#include <windows.h>

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    // Log to current working directory
    std::string logPath = "RazerBatteryTray.log";

    logFile.open(logPath, std::ios::app);
    if (!logFile.is_open()) {
        // Fallback to temp if current dir is not writable (e.g. Program Files)
        char path[MAX_PATH];
        GetTempPathA(MAX_PATH, path);
        logPath = std::string(path) + "RazerBatteryTray.log";
        logFile.open(logPath, std::ios::app);
    }
}

Logger::~Logger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

void Logger::Log(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logFile.is_open()) {
        std::time_t now = std::time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        char buf[20];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

        logFile << "[" << buf << "] [" << level << "] " << message << std::endl;
        logFile.flush();
    }
}
