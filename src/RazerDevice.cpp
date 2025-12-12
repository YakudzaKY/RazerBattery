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
    : devicePath(path), pid(pid), fileHandle(INVALID_HANDLE_VALUE),
      featureReportLength(0), inputReportLength(0), outputReportLength(0), usagePage(0), usage(0) {
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
        inputReportLength = caps.InputReportByteLength;
        outputReportLength = caps.OutputReportByteLength;
        usagePage = caps.UsagePage;
        usage = caps.Usage;
        HidD_FreePreparsedData(preparsedData);
    } else {
        featureReportLength = 0;
        inputReportLength = 0;
        outputReportLength = 0;
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

    if (request.transaction_id.id == 0) request.transaction_id.id = 0xFF;
    request.crc = CalculateCRC(&request);

    // Strategy 1: Feature Report
    if (featureReportLength > 0) {
        size_t len = featureReportLength;
        std::vector<uint8_t> buffer(len, 0);
        buffer[0] = 0x00;

        size_t copyLen = 90;
        if (len - 1 < 90) copyLen = len - 1;

        memcpy(&buffer[1], &request, copyLen);

        if (HidD_SetFeature(fileHandle, buffer.data(), (ULONG)buffer.size())) {
            Sleep(50);
            std::vector<uint8_t> responseBuffer(len, 0);
            responseBuffer[0] = 0x00;
            if (HidD_GetFeature(fileHandle, responseBuffer.data(), (ULONG)responseBuffer.size())) {
                memcpy(&response, &responseBuffer[1], 90);
                if (response.status == 0x02) return true;
            }
        } else {
             LOG_ERROR("HidD_SetFeature failed: " << GetLastError());
        }
    }

    // Strategy 2: Output Report (Control)
    {
        // Use 91 bytes or OutputReportLength
        size_t len = outputReportLength > 0 ? outputReportLength : 91;
        std::vector<uint8_t> outBuf(len, 0);
        outBuf[0] = 0x00;

        size_t copyLen = 90;
        if (len - 1 < 90) copyLen = len - 1;
        memcpy(&outBuf[1], &request, copyLen);

        if (HidD_SetOutputReport(fileHandle, outBuf.data(), (ULONG)outBuf.size())) {
            Sleep(50);
            // Read response via Input Report
            size_t inLen = inputReportLength > 0 ? inputReportLength : 91;
            std::vector<uint8_t> inBuf(inLen, 0);
            inBuf[0] = 0x00;
            if (HidD_GetInputReport(fileHandle, inBuf.data(), (ULONG)inBuf.size())) {
                 memcpy(&response, &inBuf[1], 90);
                 if (response.status == 0x02) return true;
            } else {
                 LOG_ERROR("HidD_GetInputReport failed: " << GetLastError());
            }
        } else {
             LOG_ERROR("HidD_SetOutputReport failed: " << GetLastError());

             // Strategy 3: WriteFile (Interrupt)
             // Only try if SetOutputReport failed
             DWORD bytesWritten = 0;
             if (WriteFile(fileHandle, outBuf.data(), (DWORD)outBuf.size(), &bytesWritten, NULL)) {
                 Sleep(50);

                 size_t inLen = inputReportLength > 0 ? inputReportLength : 91;
                 std::vector<uint8_t> inBuf(inLen, 0);
                 DWORD bytesRead = 0;
                 // ReadFile for Interrupt In
                 if (ReadFile(fileHandle, inBuf.data(), (DWORD)inBuf.size(), &bytesRead, NULL)) {
                      memcpy(&response, &inBuf[1], 90);
                      if (response.status == 0x02) return true;
                 } else {
                      LOG_ERROR("ReadFile failed: " << GetLastError());
                      // Try GetInputReport as backup for reading?
                      if (HidD_GetInputReport(fileHandle, inBuf.data(), (ULONG)inBuf.size())) {
                           memcpy(&response, &inBuf[1], 90);
                           if (response.status == 0x02) return true;
                      }
                 }
             } else {
                 LOG_ERROR("WriteFile failed: " << GetLastError());
             }
        }
    }

    return false;
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
