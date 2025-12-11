#include <windows.h>
#include "RazerManager.h"
#include "Logger.h"

// Global manager
RazerManager g_Manager;

#define ID_TIMER 1
#define WM_TRAYICON (WM_USER + 1)
#define IDM_EXIT 1001

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            Logger::Instance().Log("Application started");
            g_Manager.Initialize(hwnd);
            SetTimer(hwnd, ID_TIMER, 5 * 60 * 1000, NULL); // 5 minutes update
            break;

        case WM_DEVICECHANGE:
            g_Manager.HandleDeviceChange(wParam, lParam);
            break;

        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                POINT curPoint;
                GetCursorPos(&curPoint);
                SetForegroundWindow(hwnd); // Needed for the menu to disappear correctly

                HMENU hMenu = CreatePopupMenu();
                AppendMenuW(hMenu, MF_STRING, IDM_EXIT, L"Exit");

                TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, curPoint.x, curPoint.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
            }
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDM_EXIT) {
                DestroyWindow(hwnd);
            }
            break;

        case WM_TIMER:
            if (wParam == ID_TIMER) {
                g_Manager.OnTimer();
            }
            break;

        case WM_DESTROY:
            Logger::Instance().Log("Application exiting");
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {
    // Single instance check
    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"RazerBatteryTrayMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, L"The application is already running.", L"Razer Battery Tray", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    const wchar_t CLASS_NAME[] = L"RazerBatteryTrayClass";

    WNDCLASSW wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClassW(&wc);

    // Create a top-level hidden window to receive device notifications
    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Razer Battery Tray",
        WS_POPUP, // Hidden window
        0, 0, 0, 0,
        NULL, // Must be NULL to receive broadcasts
        NULL,
        hInstance,
        NULL
    );

    if (hwnd == NULL) {
        return 0;
    }

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
