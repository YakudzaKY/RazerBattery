#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <windows.h>

class Logger {
public:
    static Logger& Instance() {
        static Logger instance;
        return instance;
    }

    void Log(const std::string& message);
    void Log(const std::wstring& message);

private:
    Logger();
    ~Logger();
    std::ofstream m_file;
    std::mutex m_mutex;
};
