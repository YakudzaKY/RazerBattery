#include "RazerDevice.h"
#include <iostream>
#include <vector>

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
    // Logic derived from razermouse_driver.c
    // Most new devices use 0x1f
    // Some use 0x3f
    // Older ones 0xFF

    // Simplified mapping based on common devices.
    // Ideally this should be a comprehensive lookup.

    switch (m_pid) {
        // 0x1F Devices (Newer)
        case 0x00B6: // Basilisk V3 Pro Wired
        case 0x00B7: // Basilisk V3 Pro Wireless
        case 0x007C: // Naga Pro Wired
        case 0x007D: // Naga Pro Wireless
        case 0x0093: // DeathAdder V3 Pro Wireless
        case 0x0094: // DeathAdder V3 Pro Wired
        case 0x00A5: // Viper V2 Pro Wired
        case 0x00A6: // Viper V2 Pro Wireless
        case 0x00AB: // Cobra Pro Wireless
        case 0x00AC: // Cobra Pro Wired
            return 0x1F;

        // 0x3F Devices
        case 0x006C: // Lancehead Wired
        case 0x006D: // Lancehead Wireless
        case 0x006E: // Lancehead TE Wired
        case 0x006F: // Lancehead Wireless Receiver
        case 0x0064: // Basilisk
        case 0x0084: // DeathAdder V2
        case 0x0070: // Mamba Wireless Receiver
        case 0x0085: // DeathAdder V2 Pro Wired
        case 0x0086: // DeathAdder V2 Pro Wireless
            return 0x3F;

        // 0xFF Devices
        case 0x0044: // Mamba Wired
        case 0x0045: // Mamba Wireless
        case 0x0046: // Mamba TE Wired
        case 0x005B: // DeathAdder Elite
        case 0x005C: // Abyssus V2
        case 0x0050: // DeathAdder Chroma
        case 0x0032: // Naga Epic Chroma
        case 0x0060: // Naga Trinity
        case 0x0078: // Viper
        case 0x007A: // Viper Ultimate Wired
        case 0x007B: // Viper Ultimate Wireless
        case 0x0083: // Basilisk X HyperSpeed
        case 0x008A: // Viper Mini
            return 0xFF;

        default:
            // Default to 0x1F for unknown newer devices, or try 0xFF?
            // Let's try 0xFF as a safe default for older ones, or maybe we need to try multiple?
            // Let's default to 0xFF as it covers a lot of common ones,
            // but for "Optimized application" maybe we stick to what we know.
            return 0xFF;
    }
}

bool RazerDevice::SendRequest(RazerReport& request, RazerReport& response) {
    if (!IsConnected()) return false;

    // Ensure transaction ID is set
    if (request.transaction_id == 0) {
        request.transaction_id = GetTransactionID();
    }

    request.crc = CalculateCRC(request);

    // Prepare buffer. Windows HID expects Report ID as the first byte if using Report IDs.
    // Razer uses Report ID 0x00.
    // However, HidD_SetFeature needs the buffer to match the report.
    // Since Report ID is 0, we normally pass the data directly.
    // But some APIs expect a 0 byte prepended.
    // The Razer struct has `status` as the first byte.
    // If we assume ReportID=0, then `status` is the first byte of data.
    // For HidD_SetFeature, we typically need a buffer of [ReportID] [Data...].
    // Since ReportID is 0, we need a 0 byte then the 90 bytes of data.

    unsigned char buffer[91];
    buffer[0] = 0x00; // Report ID
    memcpy(&buffer[1], &request, 90);

    if (!HidD_SetFeature(m_hDevice, buffer, 91)) {
        // Try without the leading zero if it failed?
        // Usually feature reports always need the ID.
        return false;
    }

    // Now read response
    // Wait a bit? OpenRazer sleeps.
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
    RazerReport request = {0};
    RazerReport response = {0};

    // Class 0x07, ID 0x80, Data Size 0x02
    request.command_class = 0x07;
    request.command_id = 0x80;
    request.data_size = 0x02;

    if (SendRequest(request, response)) {
        // Value is 0-255
        int val = response.arguments[1];
        return (val * 100) / 255;
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

std::wstring RazerDevice::GetDeviceType() const {
    // Heuristic based on PID
    // 0x00xx -> Usually Keyboards or Mice
    // Let's implement a simple lookup or string "Device"
    // The prompt asks for "Ears, Mouse, etc."

    // We can use the switch case again
    switch (m_pid) {
        case 0x0044: case 0x0045: case 0x0046: // Mamba
        case 0x0050: // DeathAdder Chroma
        case 0x005B: // DA Elite
        case 0x0084: // DA V2
        case 0x0064: // Basilisk
        case 0x00B6: case 0x00B7: // Basilisk V3 Pro
        case 0x0078: case 0x007A: case 0x007B: // Viper
        case 0x008A: // Viper Mini
        case 0x00AB: case 0x00AC: // Cobra
        case 0x006C: case 0x006D: // Lancehead
            return L"Mouse";

        case 0x0054: // Kraken 7.1 Classic
        case 0x0051: // Kraken 7.1 Chroma
        case 0x0504: // Kraken V3
        case 0x051A: // Barracuda X
            return L"Headset";

        case 0x0203: // BlackWidow Chroma
        case 0x0221: // BlackWidow V3
        case 0x0257: // Huntsman V2
            return L"Keyboard";

        default:
            return L"Device";
    }
}
