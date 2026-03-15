#include "Logger.h"
#include <iostream>
#include <algorithm>
#include <cctype>
#include <windows.h>

Logger& Logger::Instance() {
    static Logger instance;
    return instance;
}

bool Logger::IsEnabled() {
    static const bool enabled = []() {
        char value[16] = {};
        const DWORD length = GetEnvironmentVariableA("RAZERBATTERY_LOG", value, static_cast<DWORD>(sizeof(value)));
        if (length == 0 || length >= sizeof(value)) {
            return false;
        }

        std::string normalized(value, length);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

        return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
    }();

    return enabled;
}

Logger::Logger() {
    if (!IsEnabled()) {
        return;
    }

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
    if (!IsEnabled()) {
        return;
    }

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
