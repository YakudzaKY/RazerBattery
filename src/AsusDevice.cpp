#include "AsusDevice.h"
#include "Logger.h"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <libusb.h>
#include <sstream>
#include <vector>

namespace {
namespace fs = std::filesystem;

const fs::path kAsusKbMsDataIni = R"(C:\ProgramData\ASUS\ARMOURY CRATE Diagnosis\ACPeripheralData\ACKbMsData.ini)";
const fs::path kAsusFrameworkRoot = R"(C:\ProgramData\ASUS\ARMOURY CRATE Diagnosis\Framework)";
constexpr std::chrono::seconds kPowerRefreshInterval(2);
constexpr std::uintmax_t kMaxServiceLogTailBytes = 128 * 1024;

std::string Trim(std::string value) {
    const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());

    if (value.size() >= 3 &&
        static_cast<unsigned char>(value[0]) == 0xEF &&
        static_cast<unsigned char>(value[1]) == 0xBB &&
        static_cast<unsigned char>(value[2]) == 0xBF) {
        value.erase(0, 3);
    }

    return value;
}

std::string ToUpperAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

std::string MakeAsusVidPid(int pid) {
    std::ostringstream oss;
    oss << "0B05" << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << pid;
    return oss.str();
}

bool MatchesAsusPidToken(const std::string& rawToken, const std::string& targetVidPid) {
    std::stringstream ss(rawToken);
    std::string token;
    const std::string targetPid = targetVidPid.size() >= 4 ? targetVidPid.substr(targetVidPid.size() - 4) : targetVidPid;

    while (std::getline(ss, token, ',')) {
        token = ToUpperAscii(Trim(token));
        if (token.empty()) continue;
        if (token == targetVidPid) return true;
        if (token.size() == 4 && token == targetPid) return true;
        if (token.size() == 8 && token.substr(4) == targetPid) return true;
    }

    return false;
}

DeviceType ParseAsusDeviceType(const std::string& rawType) {
    const std::string type = ToUpperAscii(Trim(rawType));
    if (type == "MOUSE") return DeviceType::Mouse;
    if (type == "KEYBOARD") return DeviceType::Keyboard;
    if (type == "HEADSET") return DeviceType::Headset;
    if (type == "ACCESSORY") return DeviceType::Accessory;
    return DeviceType::Unknown;
}

bool GetFallbackAsusDeviceInfo(int pid, std::string& name, DeviceType& type, std::string& modelNumber) {
    switch (pid) {
    case 0x18F4:
    case 0x1977:
    case 0x1979:
        name = "ROG SPATHA X";
        type = DeviceType::Mouse;
        modelNumber = "6521";
        return true;
    default:
        return false;
    }
}

bool TryReadAsusMetadata(const fs::path& iniPath, int pid, std::string& name, DeviceType& type,
    std::string& modelNumber, std::string& sectionName) {
    std::ifstream file(iniPath);
    if (!file.is_open()) {
        return false;
    }

    const std::string targetVidPid = MakeAsusVidPid(pid);
    std::string currentSection;
    std::string currentPid;
    std::string currentName;
    std::string currentModelNumber;
    DeviceType currentType = DeviceType::Unknown;

    const auto flushSection = [&]() {
        if (!currentSection.empty() &&
            !currentName.empty() &&
            currentType != DeviceType::Unknown &&
            MatchesAsusPidToken(currentPid, targetVidPid)) {
            name = currentName;
            type = currentType;
            modelNumber = currentModelNumber;
            sectionName = currentSection;
            return true;
        }
        return false;
    };

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            if (flushSection()) {
                return true;
            }

            currentSection = line.substr(1, line.size() - 2);
            currentPid.clear();
            currentName.clear();
            currentModelNumber.clear();
            currentType = DeviceType::Unknown;
            continue;
        }

        const size_t separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = ToUpperAscii(Trim(line.substr(0, separator)));
        const std::string value = Trim(line.substr(separator + 1));

        if (key == "PID") {
            currentPid = value;
        } else if (key == "NAME") {
            currentName = value;
        } else if (key == "DEVICEID") {
            currentModelNumber = value;
        } else if (key == "DEVICETYPE") {
            currentType = ParseAsusDeviceType(value);
        }
    }

    return flushSection();
}

fs::path FindLatestAsusServiceLog() {
    std::error_code ec;
    if (!fs::exists(kAsusFrameworkRoot, ec)) {
        return {};
    }

    fs::path bestPath;
    fs::file_time_type bestWriteTime = (fs::file_time_type::min)();

    for (fs::recursive_directory_iterator it(kAsusFrameworkRoot, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) {
            continue;
        }

        if (!it->is_regular_file(ec)) {
            continue;
        }

        const fs::path& path = it->path();
        const std::string filename = path.filename().string();
        if (filename.rfind("Service.", 0) != 0 || path.extension() != ".log") {
            continue;
        }

        const auto writeTime = it->last_write_time(ec);
        if (ec) {
            continue;
        }

        if (bestPath.empty() || writeTime > bestWriteTime) {
            bestWriteTime = writeTime;
            bestPath = path;
        }
    }

    return bestPath;
}

std::string ReadFileTail(const fs::path& path, std::uintmax_t maxBytes) {
    std::error_code ec;
    const auto size = fs::file_size(path, ec);
    if (ec) {
        return {};
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return {};
    }

    const std::uintmax_t bytesToRead = (std::min)(size, maxBytes);
    const auto startOffset = static_cast<std::streamoff>(size - bytesToRead);
    file.seekg(startOffset, std::ios::beg);

    std::string data(static_cast<size_t>(bytesToRead), '\0');
    file.read(data.data(), static_cast<std::streamsize>(bytesToRead));
    data.resize(static_cast<size_t>(file.gcount()));
    return data;
}

bool TryExtractIntField(const std::string& text, const std::string& key, int& value) {
    const size_t keyPos = text.find(key);
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t pos = keyPos + key.size();
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }

    size_t end = pos;
    while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end]))) {
        ++end;
    }

    if (end == pos) {
        return false;
    }

    value = std::stoi(text.substr(pos, end - pos));
    return true;
}

bool TryExtractBoolField(const std::string& text, const std::string& key, bool& value) {
    const size_t keyPos = text.find(key);
    if (keyPos == std::string::npos) {
        return false;
    }

    size_t pos = keyPos + key.size();
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }

    if (text.compare(pos, 4, "true") == 0) {
        value = true;
        return true;
    }
    if (text.compare(pos, 5, "false") == 0) {
        value = false;
        return true;
    }

    return false;
}

bool TryReadLatestPowerState(const std::string& modelName, const std::string& modelNumber,
    int& batteryLevel, bool& charging) {
    const fs::path logPath = FindLatestAsusServiceLog();
    if (logPath.empty()) {
        return false;
    }

    const std::string tail = ReadFileTail(logPath, kMaxServiceLogTailBytes);
    if (tail.empty()) {
        return false;
    }

    std::vector<std::string> needles;
    if (!modelNumber.empty()) {
        needles.push_back("\"modelNumber\": \"" + modelNumber + "\"");
        needles.push_back("\"pid\": \"" + modelNumber + "\"");
    }
    if (!modelName.empty()) {
        needles.push_back("\"modelName\": \"" + modelName + "\"");
    }

    for (const std::string& needle : needles) {
        size_t searchPos = tail.size();

        while (true) {
            const size_t pos = tail.rfind(needle, searchPos);
            if (pos == std::string::npos) {
                break;
            }

            const size_t windowStart = pos > 128 ? pos - 128 : 0;
            const size_t windowEnd = (std::min)(tail.size(), pos + static_cast<size_t>(384));
            const std::string window = tail.substr(windowStart, windowEnd - windowStart);

            const bool matchesName = modelName.empty() ||
                window.find("\"modelName\": \"" + modelName + "\"") != std::string::npos;
            const bool matchesModelNumber = modelNumber.empty() ||
                window.find("\"modelNumber\": \"" + modelNumber + "\"") != std::string::npos ||
                window.find("\"pid\": \"" + modelNumber + "\"") != std::string::npos;

            int parsedBattery = -1;
            bool parsedCharging = false;

            if (matchesName &&
                matchesModelNumber &&
                TryExtractIntField(window, "\"powerStatus\":", parsedBattery) &&
                TryExtractBoolField(window, "\"isCharging\":", parsedCharging)) {
                batteryLevel = std::clamp(parsedBattery, 0, 100);
                charging = parsedCharging;
                return true;
            }

            if (pos == 0) {
                break;
            }

            searchPos = pos - 1;
        }
    }

    return false;
}
}

AsusDevice::AsusDevice(libusb_device* device, int pid)
    : device(device),
      handle(nullptr),
      pid(pid),
      cachedType(DeviceType::Unknown),
      lastBatteryLevel(-1),
      lastCharging(false),
      metadataResolved(false),
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
    return RefreshMetadata();
}

void AsusDevice::Close() {
    if (handle) {
        libusb_close(handle);
        handle = nullptr;
    }
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

    if (TryReadAsusMetadata(kAsusKbMsDataIni, pid, cachedName, cachedType, cachedModelNumber, cachedSectionName)) {
        return true;
    }

    return GetFallbackAsusDeviceInfo(pid, cachedName, cachedType, cachedModelNumber);
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
    if (TryReadLatestPowerState(cachedName, cachedModelNumber, batteryLevel, charging)) {
        lastBatteryLevel = batteryLevel;
        lastCharging = charging;
    }
}
