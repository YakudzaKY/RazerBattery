#include "RazerManager.h"
#include "Logger.h"
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <algorithm>
#include <map>

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

    std::vector<std::shared_ptr<RazerDevice>> newDevicesVector;
    std::map<std::wstring, std::shared_ptr<RazerDevice>> newMap;

    // Existing devices map for reuse
    std::map<std::wstring, std::shared_ptr<RazerDevice>> existingMap;
    for (auto& d : devices) {
        // We use serial as key if available, else path
        std::wstring key = d->GetSerial();
        if (key.empty()) key = d->GetDevicePath();
        existingMap[key] = d;
    }

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
                        // It's a Razer device.
                        // Check if we should ignore it based on Type (e.g. unknown?)
                        // No, try to query it.

                        // We need to Open it with R/W to send commands.
                        CloseHandle(hFile);

                        // Create temporary device to check
                        auto dev = std::make_shared<RazerDevice>(path, attrib.ProductID);
                        if (dev->Open()) {
                            // Check if it supports battery query
                            if (dev->GetBatteryLevel() != -1) {
                                std::wstring serial = dev->GetSerial();
                                std::wstring key = serial.empty() ? path : serial;

                                if (newMap.find(key) == newMap.end()) {
                                    // Not found in new map yet (deduplicate interfaces)

                                    // Check if exists in old map
                                    if (existingMap.count(key)) {
                                        newMap[key] = existingMap[key];
                                        // Update path if needed? No, serial matches.
                                    } else {
                                        newMap[key] = dev;
                                    }
                                }
                            }
                            dev->Close();
                        }
                    } else {
                        CloseHandle(hFile);
                    }
                } else {
                    CloseHandle(hFile);
                }
            }
        }
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);

    devices.clear();
    for (auto& pair : newMap) {
        devices.push_back(pair.second);
    }
}
