#pragma once
#include "IDevice.h"
#include <windows.h>
#include <chrono>
#include <string>

struct libusb_device;

class AsusDevice : public IDevice {
public:
    AsusDevice(struct libusb_device* device, int pid);
    ~AsusDevice();

    bool Open();
    void Close();

    int getBatteryLevel() override;
    bool isCharging() override;
    const char* getDeviceName() override;
    DeviceType getDeviceType() override;

private:
    struct libusb_device* device;
    int pid;
    std::string cachedName;
    DeviceType cachedType;
    int lastBatteryLevel;
    bool lastCharging;
    bool metadataResolved;
    bool hidPathResolved;
    HANDLE hidHandle;
    std::wstring hidPath;
    std::chrono::steady_clock::time_point lastPowerRefresh;

    bool RefreshMetadata();
    bool EnsureHidOpen();
    void ResetHid();
    bool TryReadDirectPowerState(int& batteryLevel, bool& charging);
    void RefreshPowerState(bool force = false);
};
