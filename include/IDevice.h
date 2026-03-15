#pragma once

enum class DeviceType { Mouse, Keyboard, Headset, Accessory, Unknown };

class IDevice {
public:
    virtual ~IDevice() = default;

    virtual int getBatteryLevel() = 0;
    virtual bool isCharging() = 0;
    virtual const char* getDeviceName() = 0;
    virtual DeviceType getDeviceType() = 0;
};
