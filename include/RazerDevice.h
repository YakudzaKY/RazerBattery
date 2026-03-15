#pragma once
#include <string>
#include <windows.h>
#include "DeviceIds.h"
#include "RazerProtocol.h"
#include "IDevice.h"

struct libusb_device;
struct libusb_device_handle;

class RazerDevice : public IDevice {
public:
    RazerDevice(struct libusb_device* device, int pid);
    ~RazerDevice();

    bool Open();
    void Close();

    // Returns 0-100, or -1 if unknown/error.
    int getBatteryLevel() override;

    // Returns the last successfully queried battery level, or -1.
    int GetLastBatteryLevel() const { return lastBatteryLevel; }

    // Returns true if charging.
    bool isCharging() override;

    std::wstring GetSerial();
    bool IsSameDevice(struct libusb_device* other);

    int GetPID() const { return pid; }
    DeviceType getDeviceType() override;
    const char* getDeviceName() override;

private:
    struct libusb_device* device;
    struct libusb_device_handle* handle;
    int pid;
    std::wstring cachedSerial;
    int workingInterface;
    int lastBatteryLevel = -1;
    std::string cachedName;

    int QueryBatteryLevelOnce();
    bool SendRequest(razer_report& request, razer_report& response);
    unsigned char CalculateCRC(razer_report* report);
};
