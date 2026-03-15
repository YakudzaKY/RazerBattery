#pragma once

#include <array>
#include <cstddef>

struct AsusBatteryState {
    int batteryLevel = -1;
    int voltageMv = 0;
    unsigned char rawStatus = 0;
    bool charging = false;
};

std::array<unsigned char, 5> BuildSpathaBatteryQuery();
bool ParseSpathaBatteryResponse(const unsigned char* data, size_t size, AsusBatteryState& state);
