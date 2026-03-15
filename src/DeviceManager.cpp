#include "DeviceManager.h"
#include "Logger.h"
#include "RazerDevice.h"
#include "AsusDevice.h"
#include <libusb.h>
#include <map>
#include <sstream>
#include <iostream>

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

    std::map<std::wstring, std::shared_ptr<IDevice>> existingMap;
    for (auto& d : devices) {
        // This is tricky because we don't know the concrete type.
        // We'll need a way to get a unique key without calling a concrete-type method.
        // Let's assume for now that getDeviceName() + some other property will be unique.
        // This part needs more thought. For now, we'll just clear and re-add.
    }
    devices.clear();


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
                devices.push_back(newDevice);
                int batt = newDevice->getBatteryLevel();
                if (batt != -1) {
                        LOG_INFO("  Battery: " << batt << "%");
                } else {
                        LOG_ERROR("  Battery query failed.");
                }
            }
        }
    }

    libusb_free_device_list(list, 1);
    LOG_INFO("Enumeration complete. Total devices: " << devices.size());
}
