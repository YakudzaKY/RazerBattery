#pragma once
#include <string>
#include <windows.h>
#include "DeviceIds.h"
#include "RazerProtocol.h"

class RazerDevice {
public:
    RazerDevice(const std::wstring& path, int pid);
    ~RazerDevice();

    bool Open();
    void Close();

    // Returns 0-100, or -1 if unknown/error.
    int GetBatteryLevel();

    // Returns true if charging.
    bool IsCharging();

    std::wstring GetSerial();

    std::wstring GetDevicePath() const { return devicePath; }
    int GetPID() const { return pid; }
    RazerDeviceType GetType() const;
    std::wstring GetName() const;

private:
    std::wstring devicePath;
    HANDLE fileHandle;
    int pid;
    std::wstring cachedSerial;

    bool SendRequest(razer_report& request, razer_report& response);
    unsigned char CalculateCRC(razer_report* report);
};
