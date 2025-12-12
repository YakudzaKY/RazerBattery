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

void TrayIcon::Update(int batteryLevel, bool charging, RazerDeviceType type) {
    HICON hIcon = CreateBatteryIcon(batteryLevel, charging, type);
    nid.hIcon = hIcon;

    std::wstring typeStr = L"Device";
    if (type == RazerDeviceType::Mouse) typeStr = L"Mouse";
    if (type == RazerDeviceType::Headset) typeStr = L"Headset";
    if (type == RazerDeviceType::Keyboard) typeStr = L"Keyboard";

    WCHAR buf[128];
    StringCchPrintf(buf, 128, L"%s: %d%% %s", typeStr.c_str(), batteryLevel, charging ? L"(Charging)" : L"");
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

HICON TrayIcon::CreateBatteryIcon(int level, bool charging, RazerDeviceType type) {
    int w = GetSystemMetrics(SM_CXSMICON);
    int h = GetSystemMetrics(SM_CYSMICON);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, w, h);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    // Background
    RECT rect = {0, 0, w, h};
    HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0)); // Black background
    FillRect(hdcMem, &rect, brush);
    DeleteObject(brush);

    SetBkMode(hdcMem, TRANSPARENT);

    // Type Letter
    WCHAR typeChar = L'?';
    if (type == RazerDeviceType::Mouse) typeChar = L'M';
    if (type == RazerDeviceType::Headset) typeChar = L'H';
    if (type == RazerDeviceType::Keyboard) typeChar = L'K';

    HFONT hFontType = CreateFont(-8, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
    HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFontType);
    SetTextColor(hdcMem, RGB(200, 200, 200)); // Gray for type
    RECT rectTop = {0, 0, w, h/2};
    WCHAR sType[2] = {typeChar, 0};
    DrawText(hdcMem, sType, -1, &rectTop, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Level Number
    HFONT hFontLevel = CreateFont(-9, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, L"Arial");
    SelectObject(hdcMem, hFontLevel);

    // Color: Green > 50, Yellow > 20, Red < 20. Blue if charging.
    COLORREF color = RGB(0, 255, 0);
    if (level < 50) color = RGB(255, 255, 0);
    if (level < 20) color = RGB(255, 0, 0);
    if (charging) color = RGB(0, 255, 255); // Cyan for charging

    SetTextColor(hdcMem, color);
    RECT rectBot = {0, h/2, w, h};
    WCHAR sLevel[8];
    StringCchPrintf(sLevel, 8, L"%d", level);
    DrawText(hdcMem, sLevel, -1, &rectBot, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdcMem, hOldFont);
    DeleteObject(hFontType);
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
