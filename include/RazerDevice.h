#pragma once
#include <string>
#include <windows.h>
#include "DeviceIds.h"
#include "RazerProtocol.h"

struct libusb_device;
struct libusb_device_handle;

class RazerDevice {
public:
    RazerDevice(struct libusb_device* device, int pid);
    ~RazerDevice();

    bool Open();
    void Close();

    // Returns 0-100, or -1 if unknown/error.
    int GetBatteryLevel();

    // Returns the last successfully queried battery level, or -1.
    int GetLastBatteryLevel() const { return lastBatteryLevel; }

    // Returns true if charging.
    bool IsCharging();

    std::wstring GetSerial();
    bool IsSameDevice(struct libusb_device* other);

    int GetPID() const { return pid; }
    RazerDeviceType GetType() const;
    std::wstring GetName() const;

private:
    struct libusb_device* device;
    struct libusb_device_handle* handle;
    int pid;
    std::wstring cachedSerial;
    int workingInterface;
    int lastBatteryLevel = -1;

    bool SendRequest(razer_report& request, razer_report& response);
    unsigned char CalculateCRC(razer_report* report);
    int GetBatteryLevelNative();
};
