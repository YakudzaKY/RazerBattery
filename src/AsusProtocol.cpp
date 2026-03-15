#include "AsusProtocol.h"

#include <algorithm>

std::array<unsigned char, 5> BuildSpathaBatteryQuery() {
    return {0x00, 0x12, 0x07, 0x00, 0x00};
}

bool ParseSpathaBatteryResponse(const unsigned char* data, size_t size, AsusBatteryState& state) {
    if (data == nullptr || size < 11) {
        return false;
    }

    const auto expectedHeader = BuildSpathaBatteryQuery();
    if (!std::equal(expectedHeader.begin(), expectedHeader.end(), data)) {
        return false;
    }

    state.batteryLevel = std::clamp(static_cast<int>(data[5]), 0, 100);
    state.rawStatus = data[6];
    state.voltageMv = static_cast<int>(data[8]) | (static_cast<int>(data[9]) << 8);
    state.charging = data[10] != 0;
    return true;
}
