#include "Logger.h"
#include <iostream>
#include <filesystem>
#include <windows.h>

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    // Log to temp directory to avoid permission issues
    char path[MAX_PATH];
    GetTempPathA(MAX_PATH, path);
    std::string logPath = std::string(path) + "RazerBatteryTray.log";

    logFile.open(logPath, std::ios::app);
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
        char buf[20];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

        logFile << "[" << buf << "] [" << level << "] " << message << std::endl;
        logFile.flush();
    }
}
