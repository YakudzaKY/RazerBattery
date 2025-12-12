#include "RazerManager.h"
#include "Logger.h"
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <algorithm>
#include <map>
#include <iomanip>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "hid.lib")

RazerManager::RazerManager() {
}

RazerManager::~RazerManager() {
    devices.clear();
}

const std::vector<std::shared_ptr<RazerDevice>>& RazerManager::GetDevices() const {
    return devices;
}

void RazerManager::EnumerateDevices() {
    GUID hidGuid;
    HidD_GetHidGuid(&hidGuid);

    HDEVINFO hDevInfo = SetupDiGetClassDevs(&hidGuid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        LOG_ERROR("SetupDiGetClassDevs failed");
        return;
    }

    SP_DEVICE_INTERFACE_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    std::map<std::wstring, std::shared_ptr<RazerDevice>> newMap;

    // Existing devices map for reuse
    std::map<std::wstring, std::shared_ptr<RazerDevice>> existingMap;
    for (auto& d : devices) {
        std::wstring key = d->GetSerial();
        if (key.empty()) key = d->GetDevicePath();
        existingMap[key] = d;
    }

    int deviceIndex = 0;
    for (int i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &hidGuid, i, &devInfoData); i++) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInfoData, NULL, 0, &requiredSize, NULL);

        std::vector<BYTE> buffer(requiredSize);
        PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)buffer.data();
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInfoData, detailData, requiredSize, NULL, NULL)) {
            std::wstring path = (wchar_t*)detailData->DevicePath;

            HANDLE hFile = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                HIDD_ATTRIBUTES attrib;
                attrib.Size = sizeof(HIDD_ATTRIBUTES);
                if (HidD_GetAttributes(hFile, &attrib)) {
                    if (attrib.VendorID == 0x1532) {
                        // Found a Razer device
                        LOG_INFO("Found Razer Device [PID: 0x" << std::hex << attrib.ProductID << std::dec << "]");

                        // Check Usage Page
                        PHIDP_PREPARSED_DATA preparsedData;
                        if (HidD_GetPreparsedData(hFile, &preparsedData)) {
                            HIDP_CAPS caps;
                            HidP_GetCaps(preparsedData, &caps);
                            LOG_INFO("  UsagePage: 0x" << std::hex << caps.UsagePage << " Usage: 0x" << caps.Usage << std::dec);
                            HidD_FreePreparsedData(preparsedData);
                        } else {
                            LOG_INFO("  Could not get Preparsed Data.");
                        }

                        CloseHandle(hFile);

                        auto dev = std::make_shared<RazerDevice>(path, attrib.ProductID);
                        if (dev->Open()) {
                            int batt = dev->GetBatteryLevel();
                            if (batt != -1) {
                                LOG_INFO("  Battery query success: " << batt << "%");

                                std::wstring serial = dev->GetSerial();
                                std::wstring key = serial.empty() ? path : serial;

                                if (newMap.find(key) == newMap.end()) {
                                    if (existingMap.count(key)) {
                                        newMap[key] = existingMap[key];
                                        LOG_INFO("  Kept existing instance.");
                                    } else {
                                        newMap[key] = dev;
                                        LOG_INFO("  Added new instance.");
                                    }
                                } else {
                                    LOG_INFO("  Duplicate serial/interface, skipping.");
                                }
                            } else {
                                LOG_ERROR("  Battery query failed (returned -1).");
                            }
                            dev->Close();
                        } else {
                            LOG_ERROR("  Failed to open device (Access Denied?).");
                        }
                    } else {
                        CloseHandle(hFile);
                    }
                } else {
                    CloseHandle(hFile);
                }
            } else {
                // LOG_ERROR("Failed to open device handle for attributes query.");
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    devices.clear();
    for (auto& pair : newMap) {
        devices.push_back(pair.second);
    }
    LOG_INFO("Enumeration complete. Total devices: " << devices.size());
}
