#include "AsusProtocol.h"

#include <array>
#include <iostream>

namespace {

bool TestBuildSpathaBatteryQuery() {
    constexpr std::array<unsigned char, 5> expected = {0x00, 0x12, 0x07, 0x00, 0x00};
    return BuildSpathaBatteryQuery() == expected;
}

bool TestParseKnownBatteryResponse() {
    constexpr std::array<unsigned char, 65> response = {
        0x00, 0x12, 0x07, 0x00, 0x00, 0x5c, 0xff, 0x14, 0x12, 0x10, 0x00
    };

    AsusBatteryState state;
    if (!ParseSpathaBatteryResponse(response.data(), response.size(), state)) {
        return false;
    }

    return state.batteryLevel == 92 &&
        state.rawStatus == 0xff &&
        state.voltageMv == 4114 &&
        !state.charging;
}

bool TestParseAlternateVoltageResponse() {
    constexpr std::array<unsigned char, 65> response = {
        0x00, 0x12, 0x07, 0x00, 0x00, 0x5c, 0xff, 0x14, 0x0f, 0x10, 0x00
    };

    AsusBatteryState state;
    if (!ParseSpathaBatteryResponse(response.data(), response.size(), state)) {
        return false;
    }

    return state.batteryLevel == 92 &&
        state.voltageMv == 4111 &&
        !state.charging;
}

bool TestParseChargingResponse() {
    constexpr std::array<unsigned char, 65> response = {
        0x00, 0x12, 0x07, 0x00, 0x00, 0x5d, 0xff, 0x14, 0x74, 0x10, 0x01
    };

    AsusBatteryState state;
    if (!ParseSpathaBatteryResponse(response.data(), response.size(), state)) {
        return false;
    }

    return state.batteryLevel == 93 &&
        state.rawStatus == 0xff &&
        state.voltageMv == 4212 &&
        state.charging;
}

bool TestRejectsWrongHeader() {
    constexpr std::array<unsigned char, 65> response = {
        0x00, 0x12, 0x00, 0x00, 0x00, 0x22, 0x00, 0x01, 0x00, 0x05, 0x00
    };

    AsusBatteryState state;
    return !ParseSpathaBatteryResponse(response.data(), response.size(), state);
}

bool TestRejectsTruncatedResponse() {
    constexpr std::array<unsigned char, 9> response = {
        0x00, 0x12, 0x07, 0x00, 0x00, 0x5c, 0xff, 0x14, 0x12
    };

    AsusBatteryState state;
    return !ParseSpathaBatteryResponse(response.data(), response.size(), state);
}

}

int main() {
    struct TestCase {
        const char* name;
        bool (*fn)();
    };

    const TestCase tests[] = {
        {"BuildSpathaBatteryQuery", &TestBuildSpathaBatteryQuery},
        {"ParseKnownBatteryResponse", &TestParseKnownBatteryResponse},
        {"ParseAlternateVoltageResponse", &TestParseAlternateVoltageResponse},
        {"ParseChargingResponse", &TestParseChargingResponse},
        {"RejectsWrongHeader", &TestRejectsWrongHeader},
        {"RejectsTruncatedResponse", &TestRejectsTruncatedResponse},
    };

    bool allPassed = true;
    for (const auto& test : tests) {
        const bool passed = test.fn();
        std::cout << test.name << ": " << (passed ? "OK" : "FAIL") << '\n';
        allPassed = allPassed && passed;
    }

    return allPassed ? 0 : 1;
}
