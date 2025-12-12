#include "RazerManager.h"
#include "Logger.h"
#include <libusb.h>
#include <map>
#include <sstream>
#include <iostream>

RazerManager::RazerManager() : ctx(nullptr) {
    int r = libusb_init(&ctx);
    if (r < 0) {
        LOG_ERROR("libusb_init failed: " << libusb_error_name(r));
        ctx = nullptr;
    } else {
        // Optional: Set debug level
        // libusb_set_option(ctx, LIBUSB_OPTION_LOG_LEVEL, LIBUSB_LOG_LEVEL_WARNING);
    }
}

RazerManager::~RazerManager() {
    devices.clear();
    if (ctx) {
        libusb_exit(ctx);
        ctx = nullptr;
    }
}

const std::vector<std::shared_ptr<RazerDevice>>& RazerManager::GetDevices() const {
    return devices;
}

void RazerManager::EnumerateDevices() {
    if (!ctx) return;

    LOG_INFO("Enumerating devices with libusb...");

    libusb_device** list;
    ssize_t cnt = libusb_get_device_list(ctx, &list);
    if (cnt < 0) {
        LOG_ERROR("libusb_get_device_list failed: " << libusb_error_name((int)cnt));
        return;
    }

    // Map existing devices by Serial/Key to preserve instances
    std::map<std::wstring, std::shared_ptr<RazerDevice>> existingMap;
    for (auto& d : devices) {
        std::wstring s = d->GetSerial();
        if (!s.empty()) {
            existingMap[s] = d;
        } else {
            // If serial is empty (failed to read), maybe map by PID + some index?
            // Or just don't map.
            // Fallback key used below:
            std::wstringstream wss;
            wss << L"PID_" << std::hex << d->GetPID();
            existingMap[wss.str()] = d;
        }
    }

    std::map<std::wstring, std::shared_ptr<RazerDevice>> newMap;

    for (ssize_t i = 0; i < cnt; i++) {
        libusb_device* device = list[i];
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(device, &desc) == 0) {
            if (desc.idVendor == 0x1532) {
                // Found Razer Device
                LOG_INFO("Found Razer Device [PID: 0x" << std::hex << desc.idProduct << std::dec << "]");

                auto candidate = std::make_shared<RazerDevice>(device, desc.idProduct);
                if (candidate->Open()) {
                    std::wstring serial = candidate->GetSerial();
                    std::wstring key = serial;
                    if (key.empty()) {
                        std::wstringstream wss;
                        wss << L"PID_" << std::hex << desc.idProduct;
                        key = wss.str();
                    }

                    if (existingMap.count(key)) {
                        newMap[key] = existingMap[key];
                        LOG_INFO("  Kept existing instance for " << key);
                    } else {
                        newMap[key] = candidate;
                        LOG_INFO("  Added new instance for " << key);
                    }

                    // Query battery on the chosen instance
                    auto dev = newMap[key];
                    int batt = dev->GetBatteryLevel();
                    if (batt != -1) {
                         LOG_INFO("  Battery: " << batt << "%");
                    } else {
                         LOG_ERROR("  Battery query failed.");
                    }
                } else {
                    LOG_ERROR("  Failed to open device.");
                }
            }
        }
    }

    libusb_free_device_list(list, 1); // Unref devices in list

    devices.clear();
    for (auto& pair : newMap) {
        devices.push_back(pair.second);
    }
    LOG_INFO("Enumeration complete. Total devices: " << devices.size());
}
