#pragma once
#include <vector>
#include <memory>
#include "IDevice.h"

struct libusb_context;

class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    void EnumerateDevices();
    const std::vector<std::shared_ptr<IDevice>>& GetDevices() const;

private:
    std::vector<std::shared_ptr<IDevice>> devices;
    libusb_context* ctx;
};
