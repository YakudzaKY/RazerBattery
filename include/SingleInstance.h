#pragma once
#include <windows.h>
#include <string>

class SingleInstance {
public:
    SingleInstance(const std::string& name) {
        mutexHandle = CreateMutexA(NULL, TRUE, name.c_str());
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            alreadyExists = true;
        } else {
            alreadyExists = false;
        }
    }

    ~SingleInstance() {
        if (mutexHandle) {
            ReleaseMutex(mutexHandle);
            CloseHandle(mutexHandle);
        }
    }

    bool IsAnotherInstanceRunning() const {
        return alreadyExists;
    }

private:
    HANDLE mutexHandle;
    bool alreadyExists;
};
