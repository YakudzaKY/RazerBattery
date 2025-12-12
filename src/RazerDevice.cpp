#include "RazerDevice.h"
#include "Logger.h"
#include <hidsdi.h>
#include <vector>
#include <iostream>
#include <sstream>

#pragma comment(lib, "hid.lib")

RazerDevice::RazerDevice(const std::wstring& path, int pid)
    : devicePath(path), pid(pid), fileHandle(INVALID_HANDLE_VALUE) {
}

RazerDevice::~RazerDevice() {
    Close();
}

bool RazerDevice::Open() {
    fileHandle = CreateFileW(devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, 0, NULL);

    if (fileHandle == INVALID_HANDLE_VALUE) {
        // LOG_ERROR("Failed to open device: " << GetLastError());
        return false;
    }
    return true;
}

void RazerDevice::Close() {
    if (fileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(fileHandle);
        fileHandle = INVALID_HANDLE_VALUE;
    }
}

RazerDeviceType RazerDevice::GetType() const {
    return GetRazerDeviceType(pid);
}

std::wstring RazerDevice::GetName() const {
    return L"Razer Device"; // Placeholder
}

unsigned char RazerDevice::CalculateCRC(razer_report* report) {
    unsigned char crc = 0;
    unsigned char* bytes = (unsigned char*)report;
    // XOR bytes 2 to 88
    for (int i = 2; i < 88; i++) {
        crc ^= bytes[i];
    }
    return crc;
}

bool RazerDevice::SendRequest(razer_report& request, razer_report& response) {
    if (fileHandle == INVALID_HANDLE_VALUE) return false;

    // Prepare buffer: Report ID (0x00) + Report Data (90 bytes)
    std::vector<uint8_t> buffer(91, 0);
    buffer[0] = 0x00; // Report ID

    // Ensure standard values
    if (request.transaction_id.id == 0) request.transaction_id.id = 0xFF;
    request.crc = CalculateCRC(&request);

    memcpy(&buffer[1], &request, 90);

    // Send Feature Report
    if (!HidD_SetFeature(fileHandle, buffer.data(), (ULONG)buffer.size())) {
        // LOG_ERROR("HidD_SetFeature failed: " << GetLastError());
        return false;
    }

    // Wait for device to process (Wireless devices need ~50ms)
    Sleep(50);

    // Get Response
    std::vector<uint8_t> responseBuffer(91, 0);
    responseBuffer[0] = 0x00; // Report ID

    if (!HidD_GetFeature(fileHandle, responseBuffer.data(), (ULONG)responseBuffer.size())) {
        // LOG_ERROR("HidD_GetFeature failed: " << GetLastError());
        return false;
    }

    memcpy(&response, &responseBuffer[1], 90);

    if (response.status == 0x02) { // RAZER_CMD_SUCCESSFUL
        return true;
    } else {
        // LOG_ERROR("Razer command failed with status: " << (int)response.status);
        return false;
    }
}

int RazerDevice::GetBatteryLevel() {
    razer_report request = {0};
    razer_report response = {0};

    request.command_class = 0x07; // Misc
    request.command_id.id = 0x80; // Get Battery
    request.data_size = 0x02;

    if (SendRequest(request, response)) {
        // OpenRazer logic: 0-255 map to 0-100
        int level = response.arguments[1];
        return (int)((level / 255.0) * 100.0);
    }
    return -1;
}

bool RazerDevice::IsCharging() {
    razer_report request = {0};
    razer_report response = {0};

    request.command_class = 0x07;
    request.command_id.id = 0x84; // Get Charging Status
    request.data_size = 0x02;

    if (SendRequest(request, response)) {
        return response.arguments[1] == 1;
    }
    return false;
}

std::wstring RazerDevice::GetSerial() {
    if (!cachedSerial.empty()) return cachedSerial;

    razer_report request = {0};
    razer_report response = {0};
    request.command_class = 0x00;
    request.command_id.id = 0x82; // Get Serial
    request.data_size = 0x16; // 22 bytes

    if (SendRequest(request, response)) {
        char serial[23];
        memcpy(serial, response.arguments, 22);
        serial[22] = '\0';
        std::string s(serial);
        cachedSerial = std::wstring(s.begin(), s.end());
    } else {
        cachedSerial = L"";
    }
    return cachedSerial;
}
