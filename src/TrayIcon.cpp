#include "TrayIcon.h"
#include <strsafe.h>

TrayIcon::TrayIcon(HWND hwnd, int id) : m_hwnd(hwnd), m_id(id) {
    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(m_nid);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = m_id;
    m_nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    m_nid.uCallbackMessage = WM_USER + 1; // Custom message
}

TrayIcon::~TrayIcon() {
    Remove();
}

void TrayIcon::Remove() {
    Shell_NotifyIcon(NIM_DELETE, &m_nid);
}

void TrayIcon::Update(const std::wstring& type, int batteryLevel, bool isCharging) {
    m_nid.hIcon = CreateBatteryIcon(type, batteryLevel, isCharging);

    std::wstring tip;
    if (batteryLevel == -2) {
        tip = type + L": Locked (Access Denied)";
    } else if (batteryLevel < 0) {
        tip = type + L": Unknown";
    } else {
        tip = type + L": " + std::to_wstring(batteryLevel) + L"%";
        if (isCharging) tip += L" (Charging)";
    }

    StringCchCopy(m_nid.szTip, ARRAYSIZE(m_nid.szTip), tip.c_str());

    if (!Shell_NotifyIcon(NIM_MODIFY, &m_nid)) {
        Shell_NotifyIcon(NIM_ADD, &m_nid);
    }

    DestroyIcon(m_nid.hIcon); // NotifyIcon makes a copy? No, we should destroy it after update or keep it?
    // "The system makes a copy of the icon... The application must destroy the original icon when it is no longer needed."
}

void TrayIcon::ShowNoDevices() {
    m_nid.hIcon = CreateNoDeviceIcon();
    StringCchCopy(m_nid.szTip, ARRAYSIZE(m_nid.szTip), L"No Razer Devices Found");

    if (!Shell_NotifyIcon(NIM_MODIFY, &m_nid)) {
        Shell_NotifyIcon(NIM_ADD, &m_nid);
    }

    DestroyIcon(m_nid.hIcon);
}

HICON TrayIcon::CreateBatteryIcon(const std::wstring& type, int batteryLevel, bool isCharging) {
    int w = GetSystemMetrics(SM_CXSMICON);
    int h = GetSystemMetrics(SM_CYSMICON);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, w, h);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);

    // Draw background (transparent?)
    // To create a mask, we need CreateIconIndirect.
    // Let's draw black as transparent source?
    // Simpler: Draw a filled rect for battery.

    HBRUSH hBrushBg = CreateSolidBrush(RGB(0, 0, 0)); // Black background
    RECT rect = {0, 0, w, h};
    FillRect(hdcMem, &rect, hBrushBg);

    // Draw Text (Type)
    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, RGB(255, 255, 255));

    // Initial letter of type
    std::wstring letter = type.substr(0, 1);
    RECT textRect = {0, 0, w, h/2};
    DrawTextW(hdcMem, letter.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Draw Battery Bar
    RECT barRect = {2, h/2 + 2, w - 2, h - 2};
    HBRUSH hBrushFrame = CreateSolidBrush(RGB(100, 100, 100));
    FrameRect(hdcMem, &barRect, hBrushFrame);
    DeleteObject(hBrushFrame);

    InflateRect(&barRect, -1, -1);

    if (batteryLevel >= 0) {
        int barWidth = (barRect.right - barRect.left) * batteryLevel / 100;
        RECT fillRect = barRect;
        fillRect.right = fillRect.left + barWidth;

        COLORREF color = RGB(0, 255, 0);
        if (batteryLevel < 20) color = RGB(255, 0, 0);
        else if (batteryLevel < 50) color = RGB(255, 255, 0);

        if (isCharging) color = RGB(0, 255, 255); // Cyan for charging

        HBRUSH hBrushFill = CreateSolidBrush(color);
        FillRect(hdcMem, &fillRect, hBrushFill);
        DeleteObject(hBrushFill);
    } else if (batteryLevel == -2) {
        // Locked/Error
        RECT qRect = barRect;
        SetTextColor(hdcMem, RGB(255, 0, 0));
        DrawTextW(hdcMem, L"!", -1, &qRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        // Unknown status (Question mark)
        RECT qRect = barRect;
        SetTextColor(hdcMem, RGB(150, 150, 150));
        DrawTextW(hdcMem, L"?", -1, &qRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // Clean up DC
    SelectObject(hdcMem, hbmOld);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    DeleteObject(hBrushBg);

    // Create Icon
    ICONINFO ii;
    ii.fIcon = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmMask = hbm; // Use the same bitmap as mask for now (simple transparency might fail, but it's okay)
    // Actually for transparency we need a proper mask.
    // Let's create a mask where black is transparent.

    // Create an opaque mask (all black = all opaque)
    HBITMAP hbmMask = CreateBitmap(w, h, 1, 1, NULL);

    // Initialize mask to 0 (Black = Opaque for AND mask)
    // We need to select it into a DC to clear it, or use SetBitmapBits (obsolete but simple)
    // or CreateBitmap with data.
    // Let's use a temporary DC to clear it.
    HDC hdcMask = CreateCompatibleDC(NULL);
    HBITMAP hbmMaskOld = (HBITMAP)SelectObject(hdcMask, hbmMask);

    // AND mask: 0 = opaque, 1 = transparent. We want fully opaque.
    RECT maskRect = {0, 0, w, h};
    HBRUSH hBrushBlack = (HBRUSH)GetStockObject(BLACK_BRUSH);
    FillRect(hdcMask, &maskRect, hBrushBlack);

    SelectObject(hdcMask, hbmMaskOld);
    DeleteDC(hdcMask);

    ii.hbmMask = hbmMask;
    ii.hbmColor = hbm;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbm);
    DeleteObject(hbmMask);

    return hIcon;
}

HICON TrayIcon::CreateNoDeviceIcon() {
    int w = GetSystemMetrics(SM_CXSMICON);
    int h = GetSystemMetrics(SM_CYSMICON);

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, w, h);
    HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);

    HBRUSH hBrushBg = CreateSolidBrush(RGB(50, 50, 50));
    RECT rect = {0, 0, w, h};
    FillRect(hdcMem, &rect, hBrushBg);

    SetBkMode(hdcMem, TRANSPARENT);
    SetTextColor(hdcMem, RGB(255, 0, 0));
    RECT textRect = {0, 0, w, h};
    DrawTextW(hdcMem, L"X", -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdcMem, hbmOld);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    DeleteObject(hBrushBg);

    ICONINFO ii;
    ii.fIcon = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;

    // Create opaque mask
    HBITMAP hbmMask = CreateBitmap(w, h, 1, 1, NULL);
    HDC hdcMask = CreateCompatibleDC(NULL);
    HBITMAP hbmMaskOld = (HBITMAP)SelectObject(hdcMask, hbmMask);
    RECT maskRect = {0, 0, w, h};
    HBRUSH hBrushBlack = (HBRUSH)GetStockObject(BLACK_BRUSH);
    FillRect(hdcMask, &maskRect, hBrushBlack);
    SelectObject(hdcMask, hbmMaskOld);
    DeleteDC(hdcMask);

    ii.hbmMask = hbmMask;
    ii.hbmColor = hbm;

    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(hbm);
    DeleteObject(hbmMask);

    return hIcon;
}
