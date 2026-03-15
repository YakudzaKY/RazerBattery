#include "AsusDevice.h"
#include "DeviceIds.h"
#include "AsusProtocol.h"
#include "Logger.h"

#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cwctype>
#include <iomanip>
#include <libusb.h>
#include <sstream>
#include <vector>

namespace {
constexpr std::chrono::seconds kPowerRefreshInterval(2);
constexpr DWORD kHidWriteTimeoutMs = 1000;
constexpr DWORD kHidReadTimeoutMs = 1500;

std::wstring ToLowerWide(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](wchar_t ch) { return static_cast<wchar_t>(::towlower(ch)); });
    return value;
}

std::string WideToUtf8(const std::wstring& input) {
    if (input.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }

    std::string output(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()), output.data(), size, nullptr, nullptr);
    return output;
}

std::string ReadWideString(HANDLE handle, BOOLEAN(__stdcall* reader)(HANDLE, PVOID, ULONG)) {
    wchar_t buffer[256] = {};
    if (!reader(handle, buffer, static_cast<ULONG>(std::size(buffer)))) {
        return {};
    }

    return WideToUtf8(buffer);
}

std::string HexDump(const unsigned char* data, size_t size) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; ++i) {
        if (i != 0) {
            oss << ' ';
        }
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

bool SupportsDirectBatteryQuery(int pid) {
    switch (pid) {
    case USB_DEVICE_ID_ASUS_SPATHA_X:
    case USB_DEVICE_ID_ASUS_SPATHA_X_WIRED:
    case USB_DEVICE_ID_ASUS_SPATHA_X_WIRELESS:
        return true;
    default:
        return false;
    }
}

bool GetFallbackAsusDeviceInfo(int pid, std::string& name, DeviceType& type) {
    if (SupportsDirectBatteryQuery(pid)) {
        name = "ROG SPATHA X";
        type = DeviceType::Mouse;
        return true;
    }
    return false;
}

bool QueryHidCaps(HANDLE handle, HIDP_CAPS& caps) {
    PHIDP_PREPARSED_DATA preparsed = nullptr;
    if (!HidD_GetPreparsedData(handle, &preparsed)) {
        return false;
    }

    const NTSTATUS status = HidP_GetCaps(preparsed, &caps);
    HidD_FreePreparsedData(preparsed);
    return status == HIDP_STATUS_SUCCESS;
}

bool FindAsusBatteryHidPath(int pid, std::wstring& path) {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO devInfo = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devInfo == INVALID_HANDLE_VALUE) {
        return false;
    }

    const std::wstring pidNeedle = L"vid_0b05&pid_" + [] (int pidValue) {
        std::wostringstream oss;
        oss << std::hex << std::setw(4) << std::setfill(L'0') << std::nouppercase << pidValue;
        return oss.str();
    }(pid);

    SP_DEVICE_INTERFACE_DATA interfaceData = {};
    interfaceData.cbSize = sizeof(interfaceData);

    bool found = false;
    for (DWORD index = 0; SetupDiEnumDeviceInterfaces(devInfo, nullptr, &hidGuid, index, &interfaceData); ++index) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, nullptr, 0, &requiredSize, nullptr);
        if (requiredSize == 0) {
            continue;
        }

        std::vector<unsigned char> detailBuffer(requiredSize, 0);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detailBuffer.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        if (!SetupDiGetDeviceInterfaceDetailW(devInfo, &interfaceData, detail, requiredSize, nullptr, nullptr)) {
            continue;
        }

        const std::wstring candidatePath = detail->DevicePath;
        const std::wstring loweredPath = ToLowerWide(candidatePath);
        if (loweredPath.find(pidNeedle) == std::wstring::npos ||
            loweredPath.find(L"mi_00") == std::wstring::npos) {
            continue;
        }

        HANDLE handle = CreateFileW(
            candidatePath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            continue;
        }

        HIDP_CAPS caps = {};
        const bool matchesCaps = QueryHidCaps(handle, caps) &&
            caps.UsagePage == 0xff01 &&
            caps.Usage == 0x01 &&
            caps.InputReportByteLength == 65 &&
            caps.OutputReportByteLength == 65;
        CloseHandle(handle);

        if (!matchesCaps) {
            continue;
        }

        path = candidatePath;
        found = true;
        break;
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return found;
}

bool TransferHidReport(HANDLE handle, bool write, unsigned char* buffer, DWORD length, DWORD timeoutMs, DWORD& transferred) {
    transferred = 0;

    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (event == nullptr) {
        return false;
    }

    OVERLAPPED overlapped = {};
    overlapped.hEvent = event;

    const BOOL issued = write
        ? WriteFile(handle, buffer, length, &transferred, &overlapped)
        : ReadFile(handle, buffer, length, &transferred, &overlapped);

    if (!issued && GetLastError() != ERROR_IO_PENDING) {
        CloseHandle(event);
        return false;
    }

    const DWORD waitResult = WaitForSingleObject(event, timeoutMs);
    if (waitResult != WAIT_OBJECT_0) {
        CancelIoEx(handle, &overlapped);
        CloseHandle(event);
        return false;
    }

    const BOOL completed = GetOverlappedResult(handle, &overlapped, &transferred, FALSE);
    CloseHandle(event);
    return completed == TRUE;
}
}

AsusDevice::AsusDevice(libusb_device* device, int pid)
    : device(device),
      pid(pid),
      cachedType(DeviceType::Unknown),
      lastBatteryLevel(-1),
      lastCharging(false),
      metadataResolved(false),
      hidPathResolved(false),
      hidHandle(INVALID_HANDLE_VALUE),
      lastPowerRefresh((std::chrono::steady_clock::time_point::min)()) {
    if (device) {
        libusb_ref_device(device);
    }
}

AsusDevice::~AsusDevice() {
    Close();
    if (device) {
        libusb_unref_device(device);
        device = nullptr;
    }
}

bool AsusDevice::Open() {
    RefreshMetadata();
    return EnsureHidOpen() || cachedType != DeviceType::Unknown;
}

void AsusDevice::Close() {
    ResetHid();
}

int AsusDevice::getBatteryLevel() {
    RefreshPowerState();
    return lastBatteryLevel;
}

bool AsusDevice::isCharging() {
    RefreshPowerState();
    return lastCharging;
}

const char* AsusDevice::getDeviceName() {
    if (cachedName.empty() && !metadataResolved) {
        RefreshMetadata();
    }
    if (cachedName.empty()) {
        cachedName = "ASUS Peripheral";
    }
    return cachedName.c_str();
}

DeviceType AsusDevice::getDeviceType() {
    if (cachedType == DeviceType::Unknown && !metadataResolved) {
        RefreshMetadata();
    }
    return cachedType;
}

bool AsusDevice::RefreshMetadata() {
    if (metadataResolved) {
        return cachedType != DeviceType::Unknown;
    }

    metadataResolved = true;
    return GetFallbackAsusDeviceInfo(pid, cachedName, cachedType);
}

bool AsusDevice::EnsureHidOpen() {
    if (!SupportsDirectBatteryQuery(pid)) {
        return false;
    }

    if (hidHandle != INVALID_HANDLE_VALUE) {
        return true;
    }

    if (!hidPathResolved) {
        hidPathResolved = true;
        FindAsusBatteryHidPath(pid, hidPath);
        if (hidPath.empty()) {
            return false;
        }
    }

    hidHandle = CreateFileW(
        hidPath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        nullptr);
    if (hidHandle == INVALID_HANDLE_VALUE) {
        LOG_DEBUG("ASUS HID open failed for PID 0x" << std::hex << pid << std::dec
            << " path=" << WideToUtf8(hidPath) << " error=" << GetLastError());
        return false;
    }

    if (cachedName.empty() || cachedName == "ASUS Peripheral") {
        const std::string productName = ReadWideString(hidHandle, HidD_GetProductString);
        if (!productName.empty()) {
            cachedName = productName;
        }
    }

    LOG_DEBUG("Opened ASUS HID path for PID 0x" << std::hex << pid << std::dec
        << ": " << WideToUtf8(hidPath));
    return true;
}

void AsusDevice::ResetHid() {
    if (hidHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(hidHandle);
        hidHandle = INVALID_HANDLE_VALUE;
    }
}

bool AsusDevice::TryReadDirectPowerState(int& batteryLevel, bool& charging) {
    if (!SupportsDirectBatteryQuery(pid)) {
        return false;
    }

    if (!EnsureHidOpen()) {
        return false;
    }

    std::array<unsigned char, 65> query = {};
    const auto batteryQuery = BuildSpathaBatteryQuery();
    std::copy(batteryQuery.begin(), batteryQuery.end(), query.begin());

    std::array<unsigned char, 65> response = {};
    DWORD bytesTransferred = 0;

    if (!TransferHidReport(hidHandle, true, query.data(), static_cast<DWORD>(query.size()), kHidWriteTimeoutMs, bytesTransferred) ||
        bytesTransferred != query.size()) {
        LOG_DEBUG("ASUS battery write failed for PID 0x" << std::hex << pid << std::dec);
        return false;
    }

    if (!TransferHidReport(hidHandle, false, response.data(), static_cast<DWORD>(response.size()), kHidReadTimeoutMs, bytesTransferred) ||
        bytesTransferred != response.size()) {
        LOG_DEBUG("ASUS battery read failed for PID 0x" << std::hex << pid << std::dec);
        return false;
    }

    AsusBatteryState state;
    if (!ParseSpathaBatteryResponse(response.data(), response.size(), state)) {
        LOG_DEBUG("Unexpected ASUS battery response for PID 0x" << std::hex << pid << std::dec
            << ": " << HexDump(response.data(), 10));
        return false;
    }

    batteryLevel = state.batteryLevel;
    charging = state.charging;
    LOG_DEBUG("ASUS battery response PID 0x" << std::hex << pid << std::dec
        << " level=" << batteryLevel
        << " raw_status=0x" << std::hex << static_cast<int>(state.rawStatus) << std::dec
        << " voltage=" << state.voltageMv
        << " charging=" << (charging ? "true" : "false"));
    return true;
}

void AsusDevice::RefreshPowerState(bool force) {
    if (!RefreshMetadata()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (!force &&
        lastPowerRefresh != (std::chrono::steady_clock::time_point::min)() &&
        now - lastPowerRefresh < kPowerRefreshInterval) {
        return;
    }

    lastPowerRefresh = now;

    int batteryLevel = -1;
    bool charging = false;
    if (TryReadDirectPowerState(batteryLevel, charging)) {
        lastBatteryLevel = batteryLevel;
        lastCharging = charging;
        return;
    }

    ResetHid();
    if (TryReadDirectPowerState(batteryLevel, charging)) {
        lastBatteryLevel = batteryLevel;
        lastCharging = charging;
        return;
    }
}
