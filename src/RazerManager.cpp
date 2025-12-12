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

                    // Convert wstring to string for logging
                    std::string keyStr(key.begin(), key.end());

                    // Determine which object to consider (Reuse existing or use new candidate)
                    std::shared_ptr<RazerDevice> deviceToConsider = candidate;
                    bool reused = false;
                    bool physicalChange = false;

                    if (existingMap.count(key)) {
                        auto existing = existingMap[key];
                        if (existing->IsSameDevice(device)) {
                            deviceToConsider = existing;
                            reused = true;
                        } else {
                            physicalChange = true;
                        }
                    }

                    // Check for collision in the current enumeration pass (e.g. Wired + Wireless interfaces)
                    if (newMap.count(key)) {
                        auto currentInMap = newMap[key];

                        // We must query the new candidate's battery to compare
                        int battCandidate = deviceToConsider->GetBatteryLevel();
                        int battCurrent = currentInMap->GetLastBatteryLevel();

                        if (battCurrent == -1 && battCandidate != -1) {
                            newMap[key] = deviceToConsider;
                            LOG_INFO("  Replaced collision for " << keyStr << " (Better battery source found)");
                            LOG_INFO("  Battery: " << battCandidate << "%");
                        } else {
                            LOG_INFO("  Ignored collision for " << keyStr << " (Existing source preferred)");
                            if (battCandidate != -1) {
                                LOG_INFO("  (Ignored device had Battery: " << battCandidate << "%)");
                            } else {
                                LOG_ERROR("  (Ignored device battery query failed)");
                            }
                        }
                    } else {
                        // No collision, add to map
                        newMap[key] = deviceToConsider;

                        if (reused) {
                            LOG_INFO("  Kept existing instance for " << keyStr);
                        } else if (physicalChange) {
                            LOG_INFO("  Replaced instance for " << keyStr << " (Physical connection changed)");
                        } else {
                            LOG_INFO("  Added new instance for " << keyStr);
                        }

                        // Query battery
                        int batt = deviceToConsider->GetBatteryLevel();
                        if (batt != -1) {
                             LOG_INFO("  Battery: " << batt << "%");
                        } else {
                             LOG_ERROR("  Battery query failed.");
                        }
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
