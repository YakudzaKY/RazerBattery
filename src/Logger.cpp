#include "Logger.h"
#include <ctime>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <vector>

Logger::Logger() {
    wchar_t tempPath[MAX_PATH];
    DWORD res = GetTempPathW(MAX_PATH, tempPath);
    std::filesystem::path logPath;

    if (res > 0 && res < MAX_PATH) {
        logPath = std::filesystem::path(tempPath) / "RazerBatteryTray.log";
    } else {
        logPath = std::filesystem::path("debug.log"); // Fallback to local
    }

    m_file.open(logPath, std::ios::out | std::ios::app);
}

Logger::~Logger() {
    if (m_file.is_open()) {
        m_file.close();
    }
}

void Logger::Log(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_file.is_open()) {
        time_t now = time(0);
        char* dt = ctime(&now);
        std::string timeStr(dt ? dt : "Unknown time");
        // Remove newline if present
        if (!timeStr.empty() && timeStr.back() == '\n') {
            timeStr.pop_back();
        }

        m_file << "[" << timeStr << "] " << message << std::endl;
        m_file.flush();
    }
}

void Logger::Log(const std::wstring& message) {
    if (message.empty()) return;

    // Convert wstring to UTF-8 string
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &message[0], (int)message.size(), NULL, 0, NULL, NULL);
    if (size_needed <= 0) return;

    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &message[0], (int)message.size(), &strTo[0], size_needed, NULL, NULL);

    Log(strTo);
}
