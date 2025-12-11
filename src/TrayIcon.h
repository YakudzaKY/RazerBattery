#pragma once

#include <windows.h>
#include <string>
#include <shellapi.h>

class TrayIcon {
public:
    TrayIcon(HWND hwnd, int id);
    ~TrayIcon();

    void Update(const std::wstring& type, int batteryLevel, bool isCharging);
    void ShowNoDevices();
    void Remove();

private:
    HWND m_hwnd;
    int m_id;
    NOTIFYICONDATA m_nid;

    HICON CreateBatteryIcon(const std::wstring& type, int batteryLevel, bool isCharging);
    HICON CreateNoDeviceIcon();
};
