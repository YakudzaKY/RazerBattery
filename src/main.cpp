#include <windows.h>
#include "RazerManager.h"

// Global manager
RazerManager g_Manager;

#define ID_TIMER 1

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            g_Manager.Initialize(hwnd);
            SetTimer(hwnd, ID_TIMER, 5 * 60 * 1000, NULL); // 5 minutes update
            break;

        case WM_DEVICECHANGE:
            g_Manager.HandleDeviceChange(wParam, lParam);
            break;

        case WM_TIMER:
            if (wParam == ID_TIMER) {
                g_Manager.OnTimer();
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow) {
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
