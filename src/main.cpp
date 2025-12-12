#include <windows.h>
#include <dbt.h>
#include <initguid.h>
#include <hidclass.h>
#include <setupapi.h>
#include "SingleInstance.h"
#include "Logger.h"
#include "RazerManager.h"
#include "TrayIcon.h"

#define WM_TRAYICON (WM_USER + 1)
#define ID_TIMER_UPDATE 1
#define UPDATE_INTERVAL_MS 300000 // 5 minutes

// Globals
RazerManager g_Manager;
std::vector<std::unique_ptr<TrayIcon>> g_Icons;
std::unique_ptr<TrayIcon> g_PlaceholderIcon;
HWND g_hWnd = NULL;

void UpdateUI() {
    auto devices = g_Manager.GetDevices();

    if (devices.empty()) {
        g_Icons.clear();
        if (!g_PlaceholderIcon) {
            g_PlaceholderIcon = std::make_unique<TrayIcon>(g_hWnd, 99);
        }
        g_PlaceholderIcon->UpdatePlaceholder();
    } else {
        g_PlaceholderIcon.reset(); // Remove placeholder

        // Sync icons
        // Simple approach: Recreate icons if count mismatches?
        // Or reuse.
        // If we have 2 devices, and we have 2 icons, reuse.
        // If we have 3 devices, add 1.
        // If we have 1 device, remove 1.

        // Resize icons vector.
        // But removing from vector calls destructor -> Remove from tray.
        if (g_Icons.size() != devices.size()) {
            g_Icons.clear(); // Rebuild all to align IDs? Simple.
            for (size_t i = 0; i < devices.size(); i++) {
                g_Icons.push_back(std::make_unique<TrayIcon>(g_hWnd, 100 + (UINT)i));
            }
        }

        for (size_t i = 0; i < devices.size(); i++) {
            auto& dev = devices[i];
            int level = dev->GetBatteryLevel();
            bool charging = dev->IsCharging();
            // If error, maybe keep old value? Or show -1?
            // If level == -1, maybe device disconnected or locked.
            // But manager handles disconnects mostly.
            // We'll display whatever we get.

            // If level is -1, maybe assume 0 or ?
            if (level == -1) level = 0;

            g_Icons[i]->Update(level, charging, dev->GetType());
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_Manager.EnumerateDevices();
        UpdateUI();
        SetTimer(hwnd, ID_TIMER_UPDATE, UPDATE_INTERVAL_MS, NULL);

        // Register for device notifications
        {
            DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
            ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
            NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
            NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
            // HID Class GUID
            HidD_GetHidGuid(&NotificationFilter.dbcc_classguid);

            RegisterDeviceNotification(hwnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
        }
        break;

    case WM_TIMER:
        if (wParam == ID_TIMER_UPDATE) {
            // Check battery levels
            UpdateUI();
        }
        break;

    case WM_DEVICECHANGE:
        // Refresh device list
        // Delay slightly to allow drivers to load?
        // EnumerateDevices handles it.
        // Maybe sleep a bit? No, do it async?
        // We'll just run it. If it misses, the next Timer will catch it?
        // No, EnumerateDevices is only called here.
        // Let's call it.
        Sleep(100); // Small delay
        g_Manager.EnumerateDevices();
        UpdateUI();
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            // Show Context Menu (Exit)
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenu(hMenu, MF_STRING, 1001, L"Exit");
            SetForegroundWindow(hwnd);
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, NULL);
            if (cmd == 1001) {
                DestroyWindow(hwnd);
            }
            DestroyMenu(hMenu);
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    // Single Instance Check
    SingleInstance instance("Global\\RazerBatteryTray_Instance_Mutex");
    if (instance.IsAnotherInstanceRunning()) {
        MessageBox(NULL, L"Razer Battery Tray is already running.", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }

    // Window Class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"RazerBatteryTrayClass";
    RegisterClassEx(&wc);

    // Create Hidden Window
    g_hWnd = CreateWindowEx(0, L"RazerBatteryTrayClass", L"RazerBatteryTray", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
    if (!g_hWnd) return 1;

    // Message Loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
