#pragma once
#include <vector>
#include <memory>
#include "RazerDevice.h"

class RazerManager {
public:
    RazerManager();
    ~RazerManager();

    void EnumerateDevices();
    const std::vector<std::shared_ptr<RazerDevice>>& GetDevices() const;

private:
    std::vector<std::shared_ptr<RazerDevice>> devices;
};
