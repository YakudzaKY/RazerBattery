#include "RazerManager.h"
#include "Logger.h"
#include <setupapi.h>
#include <hidsdi.h>
#include <initguid.h>
#include <sstream>
#include <algorithm>

// Helper to convert GUID to string or similar, but we assume HID class GUID
DEFINE_GUID(GUID_DEVINTERFACE_HID, 0x4D1E55B2, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30);

unsigned short ParsePidFromPath(const std::wstring& path) {
    std::wstring pathLower = path;
    for (auto & c: pathLower) c = towlower(c);

    auto pos = pathLower.find(L"pid_");
    if (pos != std::wstring::npos && pos + 8 <= pathLower.length()) {
        std::wstring pidStr = pathLower.substr(pos + 4, 4);
        wchar_t* end;
        return (unsigned short)wcstol(pidStr.c_str(), &end, 16);
    }
    return 0;
}

RazerManager::RazerManager() : m_hwnd(NULL), m_hDevNotify(NULL) {
}

RazerManager::~RazerManager() {
    UnregisterNotifications();
}

void RazerManager::Initialize(HWND hwnd) {
    Logger::Instance().Log("Initializing RazerManager");
    m_hwnd = hwnd;
    m_noDeviceIcon = std::make_shared<TrayIcon>(m_hwnd, 0); // ID 0
    m_noDeviceIcon->ShowNoDevices();

    RegisterNotifications();
    EnumerateDevices();
}

void RazerManager::RegisterNotifications() {
    DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
    ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
    NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    NotificationFilter.dbcc_classguid = GUID_DEVINTERFACE_HID;

    m_hDevNotify = RegisterDeviceNotification(
        m_hwnd,
        &NotificationFilter,
        DEVICE_NOTIFY_WINDOW_HANDLE
    );
}

void RazerManager::UnregisterNotifications() {
    if (m_hDevNotify) {
        UnregisterDeviceNotification(m_hDevNotify);
        m_hDevNotify = NULL;
    }
}

void RazerManager::EnumerateDevices() {
    Logger::Instance().Log("Enumerating devices...");
    HDEVINFO hDevInfo;
    SP_DEVICE_INTERFACE_DATA devInfoData;
    PSP_DEVICE_INTERFACE_DETAIL_DATA detailData = NULL;
    SP_DEVINFO_DATA devdata = {sizeof(SP_DEVINFO_DATA)};

    hDevInfo = SetupDiGetClassDevs(
        &GUID_DEVINTERFACE_HID,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        Logger::Instance().Log("SetupDiGetClassDevs failed");
        return;
    }

    devInfoData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (int i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, NULL, &GUID_DEVINTERFACE_HID, i, &devInfoData); ++i) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInfoData, NULL, 0, &requiredSize, NULL);

        detailData = (PSP_DEVICE_INTERFACE_DETAIL_DATA)LocalAlloc(LPTR, requiredSize);
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (SetupDiGetDeviceInterfaceDetail(hDevInfo, &devInfoData, detailData, requiredSize, NULL, NULL)) {
            // Check if it's a Razer device (VID 1532)
            // We can check the path string for "vid_1532"
            std::wstring path = detailData->DevicePath;
            // Lowercase comparison
            std::wstring pathLower = path;
            for (auto & c: pathLower) c = towlower(c);

            // Check if it's a Razer device (VID 1532)
            if (pathLower.find(L"vid_1532") != std::wstring::npos) {
                Logger::Instance().Log(L"Found Razer device path: " + path);
                AddDevice(path);
            }
        }

        LocalFree(detailData);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    UpdateTrayIcons();
}

void RazerManager::AddDevice(const std::wstring& path) {
    if (m_devices.find(path) != m_devices.end()) {
        return;
    }

    unsigned short pid = ParsePidFromPath(path);

    // If we already have a working interface for this PID, skip
    if (m_activePids.find(pid) != m_activePids.end()) {
        return;
    }

    auto device = std::make_shared<RazerDevice>(path);
    if (device->Connect()) {
        if (!device->IsRazerControlInterface()) {
            return;
        }

        // We found a working interface!
        m_activePids.insert(pid);

        // Remove any locked icon for this PID
        auto itLocked = m_lockedPids.find(pid);
        if (itLocked != m_lockedPids.end()) {
            std::wstring lockedPath = itLocked->second;
            // Remove icon
            auto itIcon = m_icons.find(lockedPath);
            if (itIcon != m_icons.end()) {
                itIcon->second->Remove();
                m_icons.erase(itIcon);
            }
            m_lockedPids.erase(itLocked);
        }

        m_devices[path] = device;
        int id = m_nextIconId++;
        auto icon = std::make_shared<TrayIcon>(m_hwnd, id);
        m_icons[path] = icon;

        int level = device->GetBatteryLevel();

        if (level != -1) {
            std::wstringstream ss;
            ss << L"Device connected and verified. Battery: " << level;
            Logger::Instance().Log(ss.str());

            int charging = device->GetChargingStatus();
            icon->Update(device->GetDeviceType(), level, charging == 1);
        } else {
             // Even if battery check failed (e.g. read only 0 access), we show it as Unknown/Locked
             Logger::Instance().Log(L"Device connected but GetBatteryLevel failed: " + path);
             icon->Update(device->GetDeviceType(), level == -2 ? -2 : -1, false);
        }
    } else {
        // Connect failed
        if (GetLastError() == 5) { // Access Denied
             if (m_lockedPids.find(pid) == m_lockedPids.end()) {
                 // Add placeholder locked icon
                 m_lockedPids[pid] = path;

                 int id = m_nextIconId++;
                 auto icon = std::make_shared<TrayIcon>(m_hwnd, id);
                 m_icons[path] = icon;

                 // Show locked status
                 // We don't have a device object to get type, assume Device
                 icon->Update(L"Device", -2, false);
                 Logger::Instance().Log(L"Added locked placeholder for PID " + std::to_wstring(pid));
             }
        } else {
             Logger::Instance().Log(L"Failed to connect to device: " + path);
        }
    } else {
         Logger::Instance().Log(L"Failed to connect to device: " + path);
    }
}

void RazerManager::RemoveDevice(const std::wstring& path) {
    unsigned short pid = ParsePidFromPath(path);
    m_activePids.erase(pid);

    auto itLocked = m_lockedPids.find(pid);
    if (itLocked != m_lockedPids.end() && itLocked->second == path) {
        m_lockedPids.erase(itLocked);
    }

    auto it = m_devices.find(path);
    if (it != m_devices.end()) {
        m_devices.erase(it);
    }

    auto itIcon = m_icons.find(path);
    if (itIcon != m_icons.end()) {
        itIcon->second->Remove();
        m_icons.erase(itIcon);
    }

    UpdateTrayIcons();
}

void RazerManager::HandleDeviceChange(WPARAM wParam, LPARAM lParam) {
    if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE) {
        PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
        if (pHdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
            PDEV_BROADCAST_DEVICEINTERFACE pDevInf = (PDEV_BROADCAST_DEVICEINTERFACE)pHdr;
            std::wstring path = pDevInf->dbcc_name;

            // Check VID again
            std::wstring pathLower = path;
            for (auto & c: pathLower) c = towlower(c);

            if (pathLower.find(L"vid_1532") != std::wstring::npos) {
                if (wParam == DBT_DEVICEARRIVAL) {
                    AddDevice(path);
                } else {
                    RemoveDevice(path);
                }
            }
        }
    }
}

void RazerManager::OnTimer() {
    UpdateBatteryStatus();
}

void RazerManager::UpdateBatteryStatus() {
    for (auto& pair : m_devices) {
        auto& device = pair.second;
        if (device->IsConnected()) {
            int level = device->GetBatteryLevel();
            int charging = device->GetChargingStatus();

            if (level != -1) {
                if (m_icons.find(pair.first) != m_icons.end()) {
                    m_icons[pair.first]->Update(device->GetDeviceType(), level, charging == 1);
                }
            }
        }
    }
}

void RazerManager::UpdateTrayIcons() {
    if (m_devices.empty()) {
        m_noDeviceIcon->ShowNoDevices();
    } else {
        m_noDeviceIcon->Remove();
    }
}
