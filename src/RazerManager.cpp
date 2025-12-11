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
        // Filter by Usage Page (Must be Vendor Specific)
        if (!device->IsRazerControlInterface()) {
            // Logger::Instance().Log(L"Skipping non-control interface: " + path);
            return;
        }

        // IMPORTANT: Verify the interface really works before adding it.
        // Some devices expose multiple "Generic Desktop" interfaces, but only one is the control interface.
        // If we add all of them, we get duplicate tray icons (some working, some not).
        int level = device->GetBatteryLevel();

        // If level is -1, it means the device didn't respond correctly to the Razer protocol.
        // If it was a standard "Vendor" interface (UsagePage >= 0xFF00), we might want to keep it and show "Unknown" (maybe it's just asleep).
        // BUT if it was a "Generic" interface (UsagePage 0x1), we MUST be strict, otherwise we pick up mouse/keyboard interfaces that don't talk Razer protocol.

        // We can't easily check UsagePage here without exposing it or calling IsRazerControlInterface again/modifying it.
        // But IsRazerControlInterface is simple. Let's trust GetBatteryLevel result for filtering "Generic" interfaces.

        // Actually, let's just use the battery level check as the definitive "Is this a Razer Control Interface?" check.
        // The only risk is if a valid device is asleep and returns -1, we might skip it.
        // However, Razer devices usually respond to the battery check even if asleep (or wake up).
        // Or if they return -2 (Read Only), we should keep them.

        if (level == -1) {
             // Failed to get battery.
             // If this interface looks like a generic system interface (e.g. UsagePage 1), we should probably skip it to avoid clutter.
             // But if it's explicitly Vendor Defined (0xFF00), we might want to see the error.
             // For now, to solve the "Headset Unknown" (BlackShark) issue AND the "DeathAdder not found" (0x00B7 hidden interface) issue:

             // The DeathAdder 0x00B7 likely has a hidden interface on UsagePage 1.
             // If we now allow UsagePage 1, we will get 4+ candidates. 3 of them will fail GetBatteryLevel. 1 will succeed.
             // So we should SKIP if level == -1.

             // The BlackShark 0x0555 has a Vendor interface. It failed GetBatteryLevel before.
             // If we skip it now, the user won't see "Headset Unknown", they will see nothing.
             // Which is arguably better than a broken icon, OR worse if it means we hide a device that just needs a retry.

             // However, since we added 0x0555 to the transaction ID list, it SHOULD work now.
             // So skipping on failure is safe for now to avoid duplicates for the DeathAdder.

             // One edge case: Device is truly broken/locked. We won't see it.
             // But better than seeing 4 broken icons.

             Logger::Instance().Log(L"Device connected but GetBatteryLevel failed (skipping): " + path);
             return;
        }

        m_devices[path] = device;

        // Create an icon for it
        int id = m_nextIconId++;
        auto icon = std::make_shared<TrayIcon>(m_hwnd, id);
        m_icons[path] = icon;

        std::wstringstream ss;
        ss << L"Device connected and verified. Battery: " << level;
        Logger::Instance().Log(ss.str());

        // Initial update
        int charging = device->GetChargingStatus();
        icon->Update(device->GetDeviceType(), level, charging == 1);

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
