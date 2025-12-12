#include <windows.h>
#include <dbt.h>
#include <initguid.h>
#include <hidclass.h>
#include <setupapi.h>
#include <hidsdi.h>
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

void UpdateUI(HWND hwnd) {
    LOG_INFO("UpdateUI called. Window Handle: " << hwnd);
    auto devices = g_Manager.GetDevices();
    LOG_INFO("Device count: " << devices.size());

    if (devices.empty()) {
        g_Icons.clear();
        if (!g_PlaceholderIcon) {
            LOG_INFO("Creating placeholder icon.");
            g_PlaceholderIcon = std::make_unique<TrayIcon>(hwnd, 99);
        }
        g_PlaceholderIcon->UpdatePlaceholder();
    } else {
        if (g_PlaceholderIcon) {
            LOG_INFO("Removing placeholder icon.");
            g_PlaceholderIcon.reset();
        }

        // Resize icons vector.
        if (g_Icons.size() != devices.size()) {
            LOG_INFO("Resizing icon list from " << g_Icons.size() << " to " << devices.size());
            g_Icons.clear();
            for (size_t i = 0; i < devices.size(); i++) {
                g_Icons.push_back(std::make_unique<TrayIcon>(hwnd, 100 + (UINT)i));
            }
        }

        for (size_t i = 0; i < devices.size(); i++) {
            auto& dev = devices[i];
            int level = dev->GetBatteryLevel();
            bool charging = dev->IsCharging();

            if (level == -1) level = 0;

            // LOG_DEBUG("Updating device " << i << ": " << level << "%");
            g_Icons[i]->Update(level, charging, dev->GetType());
        }
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        LOG_INFO("WM_CREATE received. HWND: " << hwnd);
        g_Manager.EnumerateDevices();
        UpdateUI(hwnd); // Pass valid HWND
        SetTimer(hwnd, ID_TIMER_UPDATE, UPDATE_INTERVAL_MS, NULL);

        // Register for device notifications
        {
            DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
            ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
            NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
            NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
            // HID Class GUID
            HidD_GetHidGuid(&NotificationFilter.dbcc_classguid);

            HDEVNOTIFY hDevNotify = RegisterDeviceNotification(hwnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
            if (!hDevNotify) {
                LOG_ERROR("RegisterDeviceNotification failed: " << GetLastError());
            } else {
                LOG_INFO("Registered for device notifications.");
            }
        }
        break;

    case WM_TIMER:
        if (wParam == ID_TIMER_UPDATE) {
            UpdateUI(hwnd);
        }
        break;

    case WM_DEVICECHANGE:
        LOG_INFO("WM_DEVICECHANGE received.");
        Sleep(100); // Small delay
        g_Manager.EnumerateDevices();
        UpdateUI(hwnd);
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
        LOG_INFO("WM_DESTROY. Exiting.");
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

    LOG_INFO("Application starting...");

    // Window Class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"RazerBatteryTrayClass";
    RegisterClassEx(&wc);

    // Create Hidden Window
    g_hWnd = CreateWindowEx(0, L"RazerBatteryTrayClass", L"RazerBatteryTray", 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);
    if (!g_hWnd) {
        LOG_ERROR("CreateWindowEx failed: " << GetLastError());
        return 1;
    }

    LOG_INFO("Window created successfully.");

    // Message Loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
