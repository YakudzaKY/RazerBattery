#include "RazerDevice.h"
#include "Logger.h"
#include <hidsdi.h>
#include <hidpi.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "hid.lib")

RazerDevice::RazerDevice(const std::wstring& path, int pid)
    : devicePath(path), pid(pid), fileHandle(INVALID_HANDLE_VALUE), featureReportLength(0), usagePage(0), usage(0) {
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
        return false;
    }

    PHIDP_PREPARSED_DATA preparsedData;
    if (HidD_GetPreparsedData(fileHandle, &preparsedData)) {
        HIDP_CAPS caps;
        HidP_GetCaps(preparsedData, &caps);
        featureReportLength = caps.FeatureReportByteLength;
        usagePage = caps.UsagePage;
        usage = caps.Usage;
        HidD_FreePreparsedData(preparsedData);
    } else {
        featureReportLength = 91; // Default fallback
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

    // Use correct report length
    size_t len = featureReportLength > 0 ? featureReportLength : 91;

    // Razer Report is 90 bytes + 1 ID byte = 91 bytes.
    // If device reports larger size, we pad with 0.
    // If smaller, we might be in trouble, but we'll try.
    if (len < 91) {
        // LOG_ERROR("Device feature report length too small: " << len);
        // Some interfaces are not control interfaces.
        // But we try anyway?
    }

    std::vector<uint8_t> buffer(len, 0);
    buffer[0] = 0x00; // Report ID

    // Ensure standard values
    if (request.transaction_id.id == 0) request.transaction_id.id = 0xFF;
    request.crc = CalculateCRC(&request);

    size_t copyLen = 90;
    if (len - 1 < 90) copyLen = len - 1;

    memcpy(&buffer[1], &request, copyLen);

    // Send Feature Report
    if (!HidD_SetFeature(fileHandle, buffer.data(), (ULONG)buffer.size())) {
        // Only log if it's a likely control interface (UsagePage 0xFF00 or 0x1) to reduce noise?
        // No, keep verbose for now.
        LOG_ERROR("HidD_SetFeature failed: " << GetLastError() << " (Len: " << len << ")");
        return false;
    }

    // Wait for device to process (Wireless devices need ~50ms)
    Sleep(50);

    // Get Response
    std::vector<uint8_t> responseBuffer(len, 0);
    responseBuffer[0] = 0x00; // Report ID

    if (!HidD_GetFeature(fileHandle, responseBuffer.data(), (ULONG)responseBuffer.size())) {
        LOG_ERROR("HidD_GetFeature failed: " << GetLastError());
        return false;
    }

    memcpy(&response, &responseBuffer[1], 90); // Always try to read 90 bytes back

    if (response.status == 0x02) { // RAZER_CMD_SUCCESSFUL
        return true;
    } else {
        LOG_ERROR("Razer command failed with status: " << (int)response.status);
        return false;
    }
}

int RazerDevice::GetBatteryLevel() {
    uint8_t ids[] = {0xFF, 0x1F, 0x3F};

    for (uint8_t id : ids) {
        razer_report request = {0};
        razer_report response = {0};

        request.command_class = 0x07; // Misc
        request.command_id.id = 0x80; // Get Battery
        request.data_size = 0x02;
        request.transaction_id.id = id;

        if (SendRequest(request, response)) {
            int level = response.arguments[1];
            return (int)((level / 255.0) * 100.0);
        }
    }
    return -1;
}

bool RazerDevice::IsCharging() {
    uint8_t ids[] = {0xFF, 0x1F, 0x3F};

    for (uint8_t id : ids) {
        razer_report request = {0};
        razer_report response = {0};

        request.command_class = 0x07;
        request.command_id.id = 0x84; // Get Charging Status
        request.data_size = 0x02;
        request.transaction_id.id = id;

        if (SendRequest(request, response)) {
            return response.arguments[1] == 1;
        }
    }
    return false;
}

std::wstring RazerDevice::GetSerial() {
    if (!cachedSerial.empty()) return cachedSerial;

    uint8_t ids[] = {0xFF, 0x1F, 0x3F};

    for (uint8_t id : ids) {
        razer_report request = {0};
        razer_report response = {0};
        request.command_class = 0x00;
        request.command_id.id = 0x82; // Get Serial
        request.data_size = 0x16; // 22 bytes
        request.transaction_id.id = id;

        if (SendRequest(request, response)) {
            char serial[23];
            memcpy(serial, response.arguments, 22);
            serial[22] = '\0';
            std::string s(serial);
            cachedSerial = std::wstring(s.begin(), s.end());
            return cachedSerial;
        }
    }

    cachedSerial = L"";
    return cachedSerial;
}
