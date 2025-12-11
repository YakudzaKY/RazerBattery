#include "RazerManager.h"
#include "Logger.h"
#include <setupapi.h>
#include <hidsdi.h>
#include <initguid.h>
#include <sstream>

// Helper to convert GUID to string or similar, but we assume HID class GUID
DEFINE_GUID(GUID_DEVINTERFACE_HID, 0x4D1E55B2, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30);

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
        Logger::Instance().Log(L"Device already added: " + path);
        return;
    }

    auto device = std::make_shared<RazerDevice>(path);
    if (device->Connect()) {
        // Try to get battery to verify it supports the protocol
        int level = device->GetBatteryLevel();
        if (level != -1) {
            std::wstringstream ss;
            ss << L"Device connected and verified. Battery: " << level;
            Logger::Instance().Log(ss.str());

            m_devices[path] = device;

            // Create an icon for it
            // Generate a unique ID. We use a monotonic counter to avoid collisions
            // if devices are removed and re-added in different orders.
            int id = m_nextIconId++;
            auto icon = std::make_shared<TrayIcon>(m_hwnd, id);
            m_icons[path] = icon;

            // Initial update
            int charging = device->GetChargingStatus();
            icon->Update(device->GetDeviceType(), level, charging == 1);
        } else {
             Logger::Instance().Log(L"Device connected but GetBatteryLevel failed: " + path);
        }
    } else {
         Logger::Instance().Log(L"Failed to connect to device: " + path);
    }
}

void RazerManager::RemoveDevice(const std::wstring& path) {
    auto it = m_devices.find(path);
    if (it != m_devices.end()) {
        m_devices.erase(it);

        auto itIcon = m_icons.find(path);
        if (itIcon != m_icons.end()) {
            itIcon->second->Remove();
            m_icons.erase(itIcon);
        }
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
