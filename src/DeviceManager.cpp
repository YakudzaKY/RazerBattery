#include "DeviceManager.h"
#include "Logger.h"
#include "RazerDevice.h"
#include "AsusDevice.h"
#include <libusb.h>
#include <map>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace {
std::string ToLowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
}

struct RazerCandidateInfo {
    std::string logicalKey;
    int priority;
};

RazerCandidateInfo GetRazerCandidateInfo(int pid, const char* deviceName) {
    switch (pid) {
    case USB_DEVICE_ID_RAZER_DEATHADDER_V3_PRO_WIRED:
    case USB_DEVICE_ID_RAZER_DEATHADDER_V3_PRO_WIRED_ALT:
        return {"razer-deathadder-v3-pro", 30};

    case USB_DEVICE_ID_RAZER_DEATHADDER_V3_PRO_WIRELESS:
    case USB_DEVICE_ID_RAZER_DEATHADDER_V3_PRO_WIRELESS_ALT:
        return {"razer-deathadder-v3-pro", 20};

    case USB_DEVICE_ID_RAZER_DEATHADDER_V3_HYPERSPEED_WIRED:
        return {"razer-deathadder-v3-hyperspeed", 30};

    case USB_DEVICE_ID_RAZER_DEATHADDER_V3_HYPERSPEED_WIRELESS:
        return {"razer-deathadder-v3-hyperspeed", 20};

    default:
        break;
    }

    std::string name = deviceName ? deviceName : "";
    name = ToLowerCopy(name);
    if (!name.empty() && name != "razer device") {
        return {name, 10};
    }

    std::ostringstream fallback;
    fallback << "pid:0x" << std::hex << pid;
    return {fallback.str(), 0};
}

int ScoreRazerCandidate(int batteryLevel, int priority) {
    return (batteryLevel >= 0 ? 100 : 0) + priority;
}
}

DeviceManager::DeviceManager() : ctx(nullptr) {
    int r = libusb_init(&ctx);
    if (r < 0) {
        LOG_ERROR("libusb_init failed: " << libusb_error_name(r));
        ctx = nullptr;
    } else {
        // Optional: Set debug level
        // libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);
    }
}

DeviceManager::~DeviceManager() {
    devices.clear();
    if (ctx) {
        libusb_exit(ctx);
        ctx = nullptr;
    }
}

const std::vector<std::shared_ptr<IDevice>>& DeviceManager::GetDevices() const {
    return devices;
}

void DeviceManager::EnumerateDevices() {
    if (!ctx) return;

    LOG_INFO("Enumerating devices with libusb...");

    libusb_device** list;
    ssize_t cnt = libusb_get_device_list(ctx, &list);
    if (cnt < 0) {
        LOG_ERROR("libusb_get_device_list failed: " << libusb_error_name((int)cnt));
        return;
    }

    devices.clear();
    struct RazerSelection {
        size_t deviceIndex;
        int score;
    };
    std::map<std::string, RazerSelection> razerLogicalIndex;


    for (ssize_t i = 0; i < cnt; i++) {
        libusb_device* device = list[i];
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(device, &desc) == 0) {
            std::shared_ptr<IDevice> newDevice = nullptr;

            if (desc.idVendor == 0x1532) { // Razer
                LOG_INFO("Found Razer Device [PID: 0x" << std::hex << desc.idProduct << std::dec << "]");
                auto razerDev = std::make_shared<RazerDevice>(device, desc.idProduct);
                if (razerDev->Open()) {
                    newDevice = razerDev;
                }
            } else if (desc.idVendor == USB_VENDOR_ID_ASUS) {
                auto asusDev = std::make_shared<AsusDevice>(device, desc.idProduct);
                if (asusDev->Open()) {
                    LOG_INFO("Found Asus device [" << asusDev->getDeviceName()
                        << ", PID: 0x" << std::hex << desc.idProduct << std::dec << "]");
                    newDevice = asusDev;
                }
            }

            if (newDevice) {
                int batt = newDevice->getBatteryLevel();
                if (batt != -1) {
                    LOG_INFO("  Battery: " << batt << "%");
                } else {
                    LOG_ERROR("  Battery query failed.");
                }

                if (desc.idVendor == 0x1532) {
                    auto* razerDev = dynamic_cast<RazerDevice*>(newDevice.get());
                    const char* deviceName = newDevice->getDeviceName();
                    const RazerCandidateInfo info = GetRazerCandidateInfo(
                        razerDev ? razerDev->GetPID() : desc.idProduct,
                        deviceName);
                    const int score = ScoreRazerCandidate(batt, info.priority);
                    const auto existing = razerLogicalIndex.find(info.logicalKey);

                    if (existing == razerLogicalIndex.end()) {
                        razerLogicalIndex.emplace(info.logicalKey, RazerSelection{devices.size(), score});
                        devices.push_back(newDevice);
                    } else {
                        const size_t existingIndex = existing->second.deviceIndex;
                        const int existingScore = existing->second.score;

                        if (score > existingScore) {
                            LOG_INFO("Replacing duplicate Razer candidate [" << deviceName
                                << ", PID: 0x" << std::hex << desc.idProduct << std::dec
                                << "] with better transport candidate.");
                            devices[existingIndex] = newDevice;
                            existing->second.score = score;
                        } else {
                            LOG_INFO("Skipping duplicate Razer candidate [" << deviceName
                                << ", PID: 0x" << std::hex << desc.idProduct << std::dec
                                << "] because another transport already represents this device.");
                        }
                    }
                } else {
                    devices.push_back(newDevice);
                }
            }
        }
    }

    libusb_free_device_list(list, 1);
    LOG_INFO("Enumeration complete. Total devices: " << devices.size());
}
