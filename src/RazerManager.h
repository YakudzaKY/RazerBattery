#pragma once

#include <windows.h>
#include <dbt.h>
#include <map>
#include <set>
#include <string>
#include <memory>
#include "RazerDevice.h"
#include "TrayIcon.h"

class RazerManager {
public:
    RazerManager();
    ~RazerManager();

    void Initialize(HWND hwnd);
    void HandleDeviceChange(WPARAM wParam, LPARAM lParam);
    void UpdateBatteryStatus();

    // To be called from the main message loop/timer
    void OnTimer();

private:
    HWND m_hwnd;
    HDEVNOTIFY m_hDevNotify;
    std::map<std::wstring, std::shared_ptr<RazerDevice>> m_devices;
    std::map<std::wstring, std::shared_ptr<TrayIcon>> m_icons;
    std::shared_ptr<TrayIcon> m_noDeviceIcon;
    int m_nextIconId = 1;

    void EnumerateDevices();
    void AddDevice(const std::wstring& path);
    void RemoveDevice(const std::wstring& path);
    void RegisterNotifications();
    void UnregisterNotifications();

    void UpdateTrayIcons();
};
