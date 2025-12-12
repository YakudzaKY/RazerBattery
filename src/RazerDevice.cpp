#include "RazerDevice.h"
#include "Logger.h"
#include <libusb.h>
#include <vector>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <algorithm>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

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

    if (request.transaction_id.id == 0) request.transaction_id.id = 0xFF;
    request.crc = CalculateCRC(&request);

    std::vector<int> interfaces;
    if (workingInterface != -1) {
        interfaces.push_back(workingInterface);
    } else {
        struct libusb_config_descriptor *config;
        int r = libusb_get_active_config_descriptor(device, &config);
        if (r == 0) {
            for (int i = 0; i < config->bNumInterfaces; i++) {
                const struct libusb_interface *inter = &config->interface[i];
                for (int j = 0; j < inter->num_altsetting; j++) {
                    const struct libusb_interface_descriptor *interdesc = &inter->altsetting[j];

                    LOG_INFO("Interface " << (int)interdesc->bInterfaceNumber << ": Class "
                             << (int)interdesc->bInterfaceClass << ", SubClass "
                             << (int)interdesc->bInterfaceSubClass << ", Protocol "
                             << (int)interdesc->bInterfaceProtocol);

                    if (interdesc->bInterfaceClass == LIBUSB_CLASS_HID ||
                        interdesc->bInterfaceClass == LIBUSB_CLASS_VENDOR_SPEC) {
                        interfaces.push_back(interdesc->bInterfaceNumber);
                        break;
                    }
                }
            }
            libusb_free_config_descriptor(config);
        } else {
            LOG_ERROR("Failed to get active config descriptor: " << libusb_error_name(r));
        }

        // Failsafe for BlackShark V2 Pro 2023
        if (pid == 0x0555) {
             bool found = false;
             for (int i : interfaces) if (i == 3) found = true;
             if (!found) interfaces.push_back(3);
        }

        if (interfaces.empty()) {
            LOG_INFO("No HID/Vendor interfaces found. Fallback to {0, 1, 2}.");
            interfaces = {0, 1, 2};
        }
    }

    for (int iface : interfaces) {
        LOG_INFO("Trying Interface " << iface);

        bool claimed = (workingInterface == iface);
        if (!claimed) {
            int r = libusb_claim_interface(handle, iface);
            if (r == 0) {
                claimed = true;
            } else {
                LOG_ERROR("Failed to claim interface " << iface << ": " << libusb_error_name(r));
                continue;
            }
        }

        bool success = false;

        std::vector<int> report_ids = {0x00, 0x01, 0x02};

        for (int report_id : report_ids) {
            // Strategy 1: Feature Report
            int wValue = 0x0300 | report_id;

            int transferred = libusb_control_transfer(handle,
                0x21, 0x09, wValue, iface,
                (unsigned char*)&request, 90, 1000);

            if (transferred == 90) {
                Sleep(50);
                transferred = libusb_control_transfer(handle,
                    0xA1, 0x01, wValue, iface,
                    (unsigned char*)&response, 90, 1000);

                if (transferred == 90) {
                    if (response.status == 0x02) {
                        success = true;
                        LOG_INFO("Success on Interface " << iface << ", Report ID " << report_id);
                        break;
                    }
                }
            }

            // Strategy 2: Output Report + Input Report (Fallback)
            if (!success) {
                // For Output Reports, type is 0x02.
                // wValue = 0x0200 | report_id.
                // But Input Report needs type 0x01.

                int wValueOut = 0x0200 | report_id;
                int wValueIn = 0x0100 | report_id;

                transferred = libusb_control_transfer(handle,
                    0x21, 0x09, wValueOut, iface,
                    (unsigned char*)&request, 90, 1000);

                if (transferred == 90) {
                    Sleep(50);
                    transferred = libusb_control_transfer(handle,
                        0xA1, 0x01, wValueIn, iface,
                        (unsigned char*)&response, 90, 1000);

                    if (transferred == 90 && response.status == 0x02) {
                        success = true;
                        LOG_INFO("Success on Interface " << iface << ", Report ID " << report_id << " (Output/Input strategy)");
                        break;
                    }
                }
            }
        }

        if (success) {
            if (workingInterface == -1) {
                workingInterface = iface;
            }
            return true;
        } else {
            if (workingInterface == iface) {
                libusb_release_interface(handle, iface);
                workingInterface = -1;
            } else {
                libusb_release_interface(handle, iface);
            }
        }
    }

    return false;
}

int RazerDevice::GetBatteryLevelNative() {
    LOG_INFO("Attempting Native Windows HID fallback for PID " << std::hex << pid);

    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO hDevInfo = SetupDiGetClassDevs(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) return -1;

    SP_DEVICE_INTERFACE_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    int index = 0;
    while (SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &hidGuid, index++, &devInfoData)) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInfoData, NULL, 0, &requiredSize, NULL);

        std::vector<char> buffer(requiredSize);
        PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)buffer.data();
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInfoData, detailData, requiredSize, NULL, NULL)) {
            std::wstring path = detailData->DevicePath;
            std::wstring pathLower = path;
            std::transform(pathLower.begin(), pathLower.end(), pathLower.begin(), ::tolower);

            std::wstringstream wss;
            wss << L"vid_1532&pid_" << std::setfill(L'0') << std::setw(4) << std::hex << pid;
            std::wstring vidPid = wss.str();

            if (pathLower.find(vidPid) != std::wstring::npos) {
                if (pid == 0x0555) {
                    if (pathLower.find(L"mi_03") == std::wstring::npos || pathLower.find(L"col02") == std::wstring::npos) {
                        continue;
                    }
                }

                // LOG_INFO("Opening native device path"); // Verbose

                HANDLE hFile = CreateFile(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);

                if (hFile != INVALID_HANDLE_VALUE) {
                    razer_report request = {0};
                    request.command_class = 0x07;
                    request.command_id.id = 0x80;
                    request.data_size = 0x02;
                    request.transaction_id.id = 0x1F;
                    request.crc = CalculateCRC(&request);

                    BYTE reportBuffer[91];
                    memset(reportBuffer, 0, 91);
                    reportBuffer[0] = 0x00;
                    memcpy(reportBuffer + 1, &request, 90);

                    if (HidD_SetFeature(hFile, reportBuffer, 91)) {
                        Sleep(50);
                        BYTE responseBuffer[91];
                        memset(responseBuffer, 0, 91);
                        responseBuffer[0] = 0x00;

                        if (HidD_GetFeature(hFile, responseBuffer, 91)) {
                            razer_report* resp = (razer_report*)(responseBuffer + 1);
                            if (resp->status == 0x02) {
                                int level = resp->arguments[1];
                                lastBatteryLevel = (int)((level / 255.0) * 100.0);
                                LOG_INFO("Native fallback success. Battery: " << lastBatteryLevel);
                                CloseHandle(hFile);
                                SetupDiDestroyDeviceInfoList(hDevInfo);
                                return lastBatteryLevel;
                            }
                        } else {
                             LOG_ERROR("HidD_GetFeature failed: " << GetLastError());
                        }
                    } else {
                        LOG_ERROR("HidD_SetFeature failed: " << GetLastError());
                    }

                    CloseHandle(hFile);
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return -1;
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
            lastBatteryLevel = (int)((level / 255.0) * 100.0);
            return lastBatteryLevel;
        }
    }

    // Fallback for BlackShark V2 Pro 2023 if libusb failed
    if (pid == 0x0555) {
        return GetBatteryLevelNative();
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
