#pragma once
#include "IDevice.h"
#include <chrono>
#include <string>

struct libusb_device;
struct libusb_device_handle;

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
    struct libusb_device_handle* handle;
    int pid;
    std::string cachedName;
    std::string cachedSectionName;
    std::string cachedModelNumber;
    DeviceType cachedType;
    int lastBatteryLevel;
    bool lastCharging;
    bool metadataResolved;
    std::chrono::steady_clock::time_point lastPowerRefresh;

    bool RefreshMetadata();
    void RefreshPowerState(bool force = false);
};
