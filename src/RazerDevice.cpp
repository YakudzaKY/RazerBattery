#include "RazerDevice.h"
#include "Logger.h"
#include <libusb.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

RazerDevice::RazerDevice(libusb_device* device, int pid)
    : device(device), handle(nullptr), pid(pid), workingInterface(-1) {
    if (device) {
        libusb_ref_device(device);
    }
}

RazerDevice::~RazerDevice() {
    Close();
    if (device) {
        libusb_unref_device(device);
        device = nullptr;
    }
}

bool RazerDevice::IsSameDevice(libusb_device* other) {
    if (!device || !other) return false;
    return (libusb_get_bus_number(device) == libusb_get_bus_number(other)) &&
           (libusb_get_device_address(device) == libusb_get_device_address(other));
}

bool RazerDevice::Open() {
    if (handle) return true; // Already open

    if (!device) return false;

    int r = libusb_open(device, &handle);
    if (r != 0) {
        return false;
    }

    if (libusb_has_capability(LIBUSB_CAP_SUPPORTS_DETACH_KERNEL_DRIVER)) {
        libusb_set_auto_detach_kernel_driver(handle, 1);
    }

    return true;
}

void RazerDevice::Close() {
    if (handle) {
        if (workingInterface != -1) {
            libusb_release_interface(handle, workingInterface);
            workingInterface = -1;
        }
        libusb_close(handle);
        handle = nullptr;
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
    if (!handle) {
        if (!Open()) return false;
    }

    request.crc = CalculateCRC(&request);

    libusb_config_descriptor* config = nullptr;
    uint8_t interfaceCount = 0;
    if (libusb_get_active_config_descriptor(device, &config) == 0 && config) {
        interfaceCount = config->bNumInterfaces;
        // Логируем количество доступных интерфейсов, чтобы понимать, что мы пробуем
        LOG_DEBUG("Интерфейсов в активной конфигурации: " << static_cast<int>(interfaceCount));
        libusb_free_config_descriptor(config);
    } else {
        LOG_DEBUG("Не удалось прочитать активную конфигурацию, используем интерфейсы по умолчанию");
    }

    std::vector<int> interfaces;
    if (workingInterface != -1) {
        interfaces.push_back(workingInterface);
    } else {
        if (interfaceCount > 0) {
            for (uint8_t i = 0; i < interfaceCount; ++i) {
                interfaces.push_back(static_cast<int>(i));
            }
        } else {
            interfaces = {0, 1, 2, 3, 4};
        }
    }

    for (int iface : interfaces) {
        bool claimed = (workingInterface == iface);
        if (!claimed) {
            int r = libusb_claim_interface(handle, iface);
            if (r == 0) {
                claimed = true;
            } else {
                LOG_DEBUG("Не удалось захватить интерфейс " << iface << ": " << libusb_error_name(r)
                    << ". Пробуем без захвата.");
            }
        }

        bool success = false;

        // Strategy 1: Feature Report
        int transferred = libusb_control_transfer(handle,
            0x21, 0x09, 0x0300, iface,
            (unsigned char*)&request, 90, 1000);

        if (transferred == 90) {
            Sleep(50);
            transferred = libusb_control_transfer(handle,
                0xA1, 0x01, 0x0300, iface,
                (unsigned char*)&response, 90, 1000);

            if (transferred == 90 && response.status == 0x02) {
                success = true;
            }
        }

        // Strategy 2: Output Report + Input Report (Fallback)
        if (!success) {
            transferred = libusb_control_transfer(handle,
                0x21, 0x09, 0x0200, iface,
                (unsigned char*)&request, 90, 1000);

            if (transferred == 90) {
                Sleep(50);
                // Input Report (0x0100)
                transferred = libusb_control_transfer(handle,
                    0xA1, 0x01, 0x0100, iface,
                    (unsigned char*)&response, 90, 1000);

                if (transferred == 90 && response.status == 0x02) {
                    success = true;
                }
            }
        }

        if (success) {
            if (workingInterface == -1) {
                workingInterface = iface;
            }
            return true;
        } else {
            if (claimed) {
                if (workingInterface == iface) {
                    libusb_release_interface(handle, iface);
                    workingInterface = -1;
                } else {
                    libusb_release_interface(handle, iface);
                }
            }
        }
    }

    return false;
}

int RazerDevice::GetBatteryLevel() {
    struct BatteryQuery {
        uint8_t commandClass;
        uint8_t commandId;
        uint8_t dataSize;
        bool scaleFromByte;
    };

    // 0x07/0x80 — классический запрос (0-255), 0x0F/0x02 — снифф с BlackShark V2 Pro 2023 (PID 0x0555), сразу отдаёт проценты.
    const BatteryQuery queries[] = {
        {0x07, 0x80, 0x02, true},
        {0x0F, 0x02, 0x02, false},
    };

    uint8_t ids[] = {0x00, 0xFF, 0x1F, 0x3F};

    for (const auto& query : queries) {
        for (uint8_t id : ids) {
            razer_report request = {0};
            razer_report response = {0};

            request.command_class = query.commandClass;
            request.command_id.id = query.commandId;
            request.data_size = query.dataSize;
            request.transaction_id.id = id;

            if (SendRequest(request, response)) {
                int level = query.scaleFromByte
                    ? static_cast<int>(std::lround((response.arguments[1] / 255.0) * 100.0))
                    : static_cast<int>(response.arguments[1]);

                lastBatteryLevel = std::clamp(level, 0, 100);
                return lastBatteryLevel;
            }
        }
    }
    lastBatteryLevel = -1;
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

    if (!handle && !Open()) return L"";

    // Method 1: String Descriptor
    // Get Device Descriptor to find iSerialNumber index
    struct libusb_device_descriptor desc;
    if (libusb_get_device_descriptor(device, &desc) == 0) {
        if (desc.iSerialNumber > 0) {
            unsigned char data[256];
            int r = libusb_get_string_descriptor_ascii(handle, desc.iSerialNumber, data, sizeof(data));
            if (r > 0) {
                std::string s((char*)data, r);
                cachedSerial = std::wstring(s.begin(), s.end());
                return cachedSerial;
            }
        }
    }

    // Method 2: Razer Report 0x82
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

    // fallback
    std::wstringstream wss;
    wss << L"PID_" << std::hex << pid;
    cachedSerial = wss.str();

    return cachedSerial;
}
