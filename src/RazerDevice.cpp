#include "RazerDevice.h"
#include "Logger.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <algorithm>
#include <hidpi.h>

// Helper to calculate CRC
unsigned char RazerDevice::CalculateCRC(const RazerReport& report) {
    unsigned char crc = 0;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(&report);
    // XOR from byte 2 to 88 (0-indexed: starts at remaining_packets)
    // Structure offset check:
    // 0: status
    // 1: transaction_id
    // 2: remaining_packets (start)
    // ...
    // 88: last byte of arguments (end)
    // 89: crc
    // 90: reserved

    // Note: The struct packing might add padding. We should be careful.
    // However, the OpenRazer driver uses a packed struct or array.
    // Let's assume standard packing or use a byte buffer for calculation to be safe.

    for (int i = 2; i < 88; i++) {
        crc ^= p[i];
    }
    return crc;
}

RazerDevice::RazerDevice(const std::wstring& devicePath)
    : m_devicePath(devicePath), m_hDevice(INVALID_HANDLE_VALUE), m_pid(0) {
}

RazerDevice::~RazerDevice() {
    Disconnect();
}

void RazerDevice::CloseHandleSafe() {
    if (m_hDevice != INVALID_HANDLE_VALUE) {
        CloseHandle(m_hDevice);
        m_hDevice = INVALID_HANDLE_VALUE;
    }
}

bool RazerDevice::Connect() {
    m_isReadOnly = false;
    // Try Read/Write first
    m_hDevice = CreateFileW(
        m_devicePath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (m_hDevice == INVALID_HANDLE_VALUE) {
        // Try Write only (often sufficient for Feature Reports and bypasses Input security)
        m_hDevice = CreateFileW(
            m_devicePath.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
    }

    if (m_hDevice == INVALID_HANDLE_VALUE) {
        // Try Read only (just to query caps)
        m_hDevice = CreateFileW(
            m_devicePath.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
    }

    if (m_hDevice == INVALID_HANDLE_VALUE) {
        // Try 0 (Attributes only)
        m_hDevice = CreateFileW(
            m_devicePath.c_str(),
            0,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        if (m_hDevice != INVALID_HANDLE_VALUE) {
            m_isReadOnly = true;
            Logger::Instance().Log(L"Connected in limited mode (0 access) for " + m_devicePath);
        }
    }

    if (m_hDevice == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        std::wstringstream ss;
        ss << L"CreateFile failed. Error: " << err << L" Path: " << m_devicePath;
        Logger::Instance().Log(ss.str());
        return false;
    }

    HIDD_ATTRIBUTES attrib;
    attrib.Size = sizeof(attrib);
    if (HidD_GetAttributes(m_hDevice, &attrib)) {
        m_pid = attrib.ProductID;
        // Verify VID just in case
        if (attrib.VendorID != USB_VENDOR_ID_RAZER) {
            CloseHandleSafe();
            return false;
        }
    } else {
        Logger::Instance().Log("HidD_GetAttributes failed");
        CloseHandleSafe();
        return false;
    }

    return true;
}

void RazerDevice::Disconnect() {
    CloseHandleSafe();
}

bool RazerDevice::IsConnected() const {
    return m_hDevice != INVALID_HANDLE_VALUE;
}

std::wstring RazerDevice::GetDevicePath() const {
    return m_devicePath;
}

unsigned short RazerDevice::GetPID() const {
    return m_pid;
}

unsigned char RazerDevice::GetTransactionID() const {
    // Comprehensive list based on OpenRazer driver
    switch (m_pid) {
        // 0x3F Devices
        case 0x0059: // Lancehead Wired
        case 0x005A: // Lancehead Wireless
        case 0x0072: // Mamba Wireless Receiver
        case 0x0073: // Mamba Wireless Wired
        case 0x007C: // DeathAdder V2 Pro Wired
        case 0x007D: // DeathAdder V2 Pro Wireless
            return 0x3F;

        // 0x1F Devices
        case 0x008F: // Naga Pro Wired
        case 0x0090: // Naga Pro Wireless
        case 0x0088: // Basilisk Ultimate Receiver
        case 0x0086: // Basilisk Ultimate Wired
        case 0x006F: // Lancehead Wireless Receiver
        case 0x0070: // Lancehead Wireless Wired
        case 0x0062: // Atheris Receiver
        case 0x0094: // Orochi V2 Receiver
        case 0x0095: // Orochi V2 Bluetooth
        case 0x0077: // Pro Click Receiver
        case 0x0080: // Pro Click Wired
        case 0x009C: // DeathAdder V2 X HyperSpeed
        case 0x00A6: // Viper V2 Pro Wireless
        case 0x00A5: // Viper V2 Pro Wired
        case 0x009E: // Viper Mini SE Wired
        case 0x009F: // Viper Mini SE Wireless
        case 0x00B0: // Cobra Pro Wireless
        case 0x00AF: // Cobra Pro Wired
        case 0x00B7: // DeathAdder V3 Pro Wireless
        case 0x00B6: // DeathAdder V3 Pro Wired
        case 0x00C3: // DeathAdder V3 Pro Wireless Alt
        case 0x00C2: // DeathAdder V3 Pro Wired Alt
        case 0x00C4: // DeathAdder V3 HyperSpeed Wired
        case 0x00C5: // DeathAdder V3 HyperSpeed Wireless
        case 0x00B3: // HyperPolling Wireless Dongle
        case 0x00AA: // Basilisk V3 Pro Wired
        case 0x00AB: // Basilisk V3 Pro Wireless
        case 0x00CB: // Basilisk V3 35K
        case 0x00CC: // Basilisk V3 Pro 35K Wired
        case 0x00CD: // Basilisk V3 Pro 35K Wireless
        case 0x00D6: // Basilisk V3 Pro 35K Phantom Green Edition Wired
        case 0x00D7: // Basilisk V3 Pro 35K Phantom Green Edition Wireless
        case 0x009A: // Pro Click Mini Receiver
        case 0x00A7: // Naga V2 Pro Wired
        case 0x00A8: // Naga V2 Pro Wireless
        case 0x00B4: // Naga V2 HyperSpeed Receiver
        case 0x00B8: // Viper V3 HyperSpeed
        case 0x00B9: // Basilisk V3 X HyperSpeed
        case 0x00BE: // DeathAdder V4 Pro Wired
        case 0x00BF: // DeathAdder V4 Pro Wireless
        case 0x00C0: // Viper V3 Pro Wired
        case 0x00C1: // Viper V3 Pro Wireless
        case 0x00C8: // Pro Click V2 Vertical Edition Wireless
        case 0x00C7: // Pro Click V2 Vertical Edition Wired
        case 0x00D0: // Pro Click V2 Wired
        case 0x00D1: // Pro Click V2 Wireless
        case 0x0555: // BlackShark V2 2023
            return 0x1F;

        // 0xFF Devices
        case 0x001F: // Naga Epic
        case 0x0024: // Mamba 2012 Wired
        case 0x0025: // Mamba 2012 Wireless
        case 0x0032: // Ouroboros
        case 0x003E: // Naga Epic Chroma
        case 0x003F: // Naga Epic Chroma Dock
        case 0x0044: // Mamba Wired
        case 0x0045: // Mamba Wireless
        case 0x007A: // Viper Ultimate Wired
        case 0x007B: // Viper Ultimate Wireless
        case 0x0083: // Basilisk X HyperSpeed
            return 0xFF;

        default:
             return 0xFF;
    }
}

bool RazerDevice::SendRequest(RazerReport& request, RazerReport& response) {
    if (!IsConnected()) {
        Logger::Instance().Log(L"SendRequest: Not connected " + m_devicePath);
        return false;
    }

    // Ensure transaction ID is set
    if (request.transaction_id == 0) {
        if (m_transactionId != 0) {
            request.transaction_id = m_transactionId;
        } else {
            request.transaction_id = GetTransactionID();
        }
    }

    request.crc = CalculateCRC(request);

    unsigned char buffer[91];
    buffer[0] = 0x00; // Report ID
    memcpy(&buffer[1], &request, 90);

    if (!HidD_SetFeature(m_hDevice, buffer, 91)) {
        return false;
    }

    // Now read response
    Sleep(20); // 20ms

    unsigned char inBuffer[91];
    inBuffer[0] = 0x00; // Report ID

    if (!HidD_GetFeature(m_hDevice, inBuffer, 91)) {
        return false;
    }

    memcpy(&response, &inBuffer[1], 90);

    // Validate response
    if (response.remaining_packets != request.remaining_packets ||
        response.command_class != request.command_class ||
        response.command_id != request.command_id) {
        return false;
    }

    return true;
}

int RazerDevice::GetBatteryLevel() {
    if (m_isReadOnly) return -2;

    // Try multiple transaction IDs if default fails
    // If we already have a working ID, try it first
    std::vector<unsigned char> ids;
    if (m_transactionId != 0) {
        ids.push_back(m_transactionId);
    } else {
        ids.push_back(GetTransactionID());
    }

    // Add fallbacks
    if (std::find(ids.begin(), ids.end(), 0x3F) == ids.end()) ids.push_back(0x3F);
    if (std::find(ids.begin(), ids.end(), 0x1F) == ids.end()) ids.push_back(0x1F);
    if (std::find(ids.begin(), ids.end(), 0xFF) == ids.end()) ids.push_back(0xFF);
    if (std::find(ids.begin(), ids.end(), 0x80) == ids.end()) ids.push_back(0x80);
    if (std::find(ids.begin(), ids.end(), 0x08) == ids.end()) ids.push_back(0x08);

    for (unsigned char id : ids) {
        RazerReport request = {0};
        RazerReport response = {0};

        request.transaction_id = id;
        request.command_class = 0x07;
        request.command_id = 0x80;
        request.data_size = 0x02;

        if (SendRequest(request, response)) {
            // Cache the working ID
            m_transactionId = id;

            // Value is 0-255
            int val = response.arguments[1];
            return (val * 100) / 255;
        }
    }

    return -1;
}

int RazerDevice::GetChargingStatus() {
    RazerReport request = {0};
    RazerReport response = {0};

    // Class 0x07, ID 0x84, Data Size 0x02
    request.command_class = 0x07;
    request.command_id = 0x84;
    request.data_size = 0x02;

    if (SendRequest(request, response)) {
        return response.arguments[1];
    }
    return -1;
}

bool RazerDevice::IsRazerControlInterface() {
    if (!IsConnected()) return false;

    PHIDP_PREPARSED_DATA preparsedData;
    if (!HidD_GetPreparsedData(m_hDevice, &preparsedData)) {
        Logger::Instance().Log(L"HidD_GetPreparsedData failed for " + m_devicePath);
        return false;
    }

    HIDP_CAPS caps;
    NTSTATUS status = HidP_GetCaps(preparsedData, &caps);

    HidD_FreePreparsedData(preparsedData);

    if (status != HIDP_STATUS_SUCCESS) {
        Logger::Instance().Log(L"HidP_GetCaps failed for " + m_devicePath);
        return false;
    }

    std::wstringstream ss;
    ss << L"Device Caps: UsagePage=0x" << std::hex << caps.UsagePage << L" Usage=0x" << caps.Usage
       << L" FeatureReportByteLength=" << caps.FeatureReportByteLength
       << L" Path=" << m_devicePath;
    Logger::Instance().Log(ss.str());

    m_usagePage = caps.UsagePage;

    // Filter for Vendor Defined Usage Pages (0xFF00 - 0xFFFF)
    if (caps.UsagePage >= 0xFF00) {
        return true;
    }

    // Allow Generic Desktop (0x1) with Usage 0 (Undefined) - common fallback for some devices
    if (caps.UsagePage == 0x1 && caps.Usage == 0x0) {
        if (caps.FeatureReportByteLength >= 90) {
            return true;
        } else {
             Logger::Instance().Log(L"Skipping Generic interface due to small report length: " + m_devicePath);
        }
    }

    return false;
}

bool RazerDevice::IsGeneric() const {
    return m_usagePage == 0x1;
}

std::wstring RazerDevice::GetDeviceType() const {
    switch (m_pid) {
        // Headsets
        case 0x0054: // Kraken Classic
        case 0x0201: // Kraken Classic Alt
        case 0x0051: // Kraken 7.1 Chroma
        case 0x0504: // Kraken 7.1 V2
        case 0x0527: // Kraken Ultimate
        case 0x0535: // Kraken Kitty V2
        case 0x051A: // Barracuda X
        case 0x0555: // BlackShark V2 2023
            return L"Headset";

        // Keyboards (Examples)
        case 0x0203: // BlackWidow Chroma
        case 0x0221: // BlackWidow V3
        case 0x0257: // Huntsman V2
            return L"Keyboard";

        default:
            // Most devices handled by this class are mice (from razermouse driver)
            return L"Mouse";
    }
}
