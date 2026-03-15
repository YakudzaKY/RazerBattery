#include "TrayIcon.h"
#include <tchar.h>
#include <strsafe.h>
#include "Logger.h"

#define WM_TRAYICON (WM_USER + 1)

TrayIcon::TrayIcon(HWND hwnd, UINT id) : hwnd(hwnd), id(id) {
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = id;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
}

TrayIcon::~TrayIcon() {
    Remove();
}

void TrayIcon::Remove() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void TrayIcon::UpdatePlaceholder() {
    HICON hIcon = CreatePlaceholderIcon();
    nid.hIcon = hIcon;
    StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), L"No Razer Devices Found");

    // Try to modify first, if fails, add
    if (!Shell_NotifyIcon(NIM_MODIFY, &nid)) {
        if (!Shell_NotifyIcon(NIM_ADD, &nid)) {
             LOG_ERROR("Shell_NotifyIcon failed for ID " << id << ": " << GetLastError());
        }
    }

    DestroyIcon(hIcon);
}

void TrayIcon::Update(int batteryLevel, bool charging, DeviceType type) {
    HICON hIcon = CreateBatteryIcon(batteryLevel, charging, type);
    nid.hIcon = hIcon;

    std::wstring typeStr = L"Device";
    if (type == DeviceType::Mouse) typeStr = L"Mouse";
    if (type == DeviceType::Headset) typeStr = L"Headset";
    if (type == DeviceType::Keyboard) typeStr = L"Keyboard";
    if (type == DeviceType::Accessory) typeStr = L"Accessory";

    WCHAR buf[128];
    if (batteryLevel < 0) {
        StringCchPrintf(buf, 128, L"%s: - (Offline)", typeStr.c_str());
    } else {
        StringCchPrintf(buf, 128, L"%s: %d%% %s", typeStr.c_str(), batteryLevel, charging ? L"(Charging)" : L"");
    }
    StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), buf);

    if (!Shell_NotifyIcon(NIM_MODIFY, &nid)) {
        if (!Shell_NotifyIcon(NIM_ADD, &nid)) {
             LOG_ERROR("Shell_NotifyIcon failed for ID " << id << ": " << GetLastError());
        }
    }

    DestroyIcon(hIcon);
}

HICON TrayIcon::CreatePlaceholderIcon() {
    int w = GetSystemMetrics(SM_CXSMICON);
    int h = GetSystemMetrics(SM_CYSMICON);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, w, h);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    // Draw
    RECT rect = {0, 0, w, h};
    HBRUSH brush = CreateSolidBrush(RGB(50, 50, 50));
    FillRect(hdcMem, &rect, brush);
    DeleteObject(brush);

    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, RGB(200, 200, 200));

    HFONT hFont = CreateFont(-10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);

    DrawText(hdcMem, L"No", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdcMem, hOldFont);
    DeleteObject(hFont);

    // Create Icon
    ICONINFO ii = {0};
    ii.fIcon = TRUE;
    ii.hbmMask = hBitmap;
    ii.hbmColor = hBitmap;
    HICON hIcon = CreateIconIndirect(&ii);

    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return hIcon;
}

HICON TrayIcon::CreateBatteryIcon(int level, bool charging, DeviceType type) {
    int w = GetSystemMetrics(SM_CXSMICON);
    int h = GetSystemMetrics(SM_CYSMICON);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, w, h);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    // Background color by device type (all dark for tray readability).
    COLORREF bgColor = RGB(12, 12, 12);
    switch (type) {
    case DeviceType::Mouse:
        bgColor = RGB(8, 26, 8);   // dark green
        break;
    case DeviceType::Headset:
        bgColor = RGB(8, 16, 30);  // dark blue
        break;
    case DeviceType::Keyboard:
        bgColor = RGB(28, 18, 8);  // dark amber
        break;
    case DeviceType::Accessory:
        bgColor = RGB(18, 10, 24); // dark violet
        break;
    default:
        bgColor = RGB(12, 12, 12); // unknown
        break;
    }

    // Fill background.
    RECT rect = {0, 0, w, h};
    HBRUSH brush = CreateSolidBrush(bgColor);
    FillRect(hdcMem, &rect, brush);
    DeleteObject(brush);

    SetBkMode(hdcMem, TRANSPARENT);

    // Large centered battery number.
    const bool isOffline = (level < 0);
    int levelFontHeight = h - 2;
    if (isOffline || level >= 100) {
        levelFontHeight = h - 6;
    } else if (level >= 10) {
        levelFontHeight = h - 4;
    }
    if (levelFontHeight < 8) levelFontHeight = 8;

    HFONT hFontLevel = CreateFont(-levelFontHeight, 0, 0, 0, FW_HEAVY, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, L"Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFontLevel);

    // Palette tuned for readability in 16x16 tray icons.
    COLORREF color = RGB(120, 255, 120);
    if (isOffline) {
        color = RGB(150, 150, 150);
    } else if (charging) {
        color = RGB(90, 220, 255);
    } else if (level < 20) {
        color = RGB(255, 90, 90);
    } else if (level < 50) {
        color = RGB(255, 220, 90);
    }

    SetTextColor(hdcMem, color);
    WCHAR sLevel[8];
    if (isOffline) {
        StringCchCopy(sLevel, ARRAYSIZE(sLevel), L"-");
    } else {
        StringCchPrintf(sLevel, ARRAYSIZE(sLevel), L"%d", level);
    }
    RECT rectLevel = {0, 1, w, h + 1};
    DrawText(hdcMem, sLevel, -1, &rectLevel, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdcMem, hOldFont);
    DeleteObject(hFontLevel);

    // Create Icon
    ICONINFO ii = {0};
    ii.fIcon = TRUE;
    ii.hbmMask = hBitmap;
    ii.hbmColor = hBitmap;
    HICON hIcon = CreateIconIndirect(&ii);

    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return hIcon;
}
