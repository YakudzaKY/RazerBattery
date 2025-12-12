#pragma once
#include <windows.h>
#include <string>
#include "DeviceIds.h"

class TrayIcon {
public:
    TrayIcon(HWND hwnd, UINT id);
    ~TrayIcon();

    void Update(int batteryLevel, bool charging, RazerDeviceType type);
    void Remove();
    void UpdatePlaceholder();

private:
    HWND hwnd;
    UINT id;
    NOTIFYICONDATA nid;

    HICON CreateBatteryIcon(int level, bool charging, RazerDeviceType type);
    HICON CreatePlaceholderIcon();
};
