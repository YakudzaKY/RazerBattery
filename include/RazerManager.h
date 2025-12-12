#pragma once
#include <vector>
#include <memory>
#include "RazerDevice.h"

struct libusb_context;

class RazerManager {
public:
    RazerManager();
    ~RazerManager();

    void EnumerateDevices();
    const std::vector<std::shared_ptr<RazerDevice>>& GetDevices() const;

private:
    std::vector<std::shared_ptr<RazerDevice>> devices;
    libusb_context* ctx;
};
