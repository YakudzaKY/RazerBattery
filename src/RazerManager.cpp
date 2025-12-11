#include "RazerManager.h"
#include <setupapi.h>
#include <hidsdi.h>
#include <initguid.h>

// Helper to convert GUID to string or similar, but we assume HID class GUID
DEFINE_GUID(GUID_DEVINTERFACE_HID, 0x4D1E55B2, 0xF16F, 0x11CF, 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30);

RazerManager::RazerManager() : m_hwnd(NULL), m_hDevNotify(NULL) {
}

RazerManager::~RazerManager() {
    UnregisterNotifications();
}

void RazerManager::Initialize(HWND hwnd) {
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

    if (hDevInfo == INVALID_HANDLE_VALUE) return;

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
                // We generally want interface 0 (mi_00) for control, but we'll try adding
                // any interface matching the VID. AddDevice handles duplicate checks and
                // connectivity verification (GetBatteryLevel), so if we grab a wrong
                // interface (like a keyboard/mouse endpoint that doesn't support feature reports),
                // the handshake will fail gracefully.
                AddDevice(path);
            }
        }

        LocalFree(detailData);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    UpdateTrayIcons();
}

void RazerManager::AddDevice(const std::wstring& path) {
    if (m_devices.find(path) != m_devices.end()) return;

    auto device = std::make_shared<RazerDevice>(path);
    if (device->Connect()) {
        // Try to get battery to verify it supports the protocol
        if (device->GetBatteryLevel() != -1) {
            m_devices[path] = device;

            // Create an icon for it
            // Generate a unique ID. We use a monotonic counter to avoid collisions
            // if devices are removed and re-added in different orders.
            int id = m_nextIconId++;
            auto icon = std::make_shared<TrayIcon>(m_hwnd, id);
            m_icons[path] = icon;

            // Initial update
            int level = device->GetBatteryLevel();
            int charging = device->GetChargingStatus();
            icon->Update(device->GetDeviceType(), level, charging == 1);
        }
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
