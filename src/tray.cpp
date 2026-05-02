// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  tray.cpp  -  System tray icon via Shell_NotifyIcon
//
//  The GLFW window procedure is subclassed using SetWindowSubclass
//  (commctrl) so we can intercept the WM_TRAY_ICON callback
//  message that Shell_NotifyIcon posts to our window.
//
//  All Win32 messages are dispatched on the main thread through
//  glfwPollEvents / glfwWaitEventsTimeout, so no locking is
//  needed for the atomic flags (same-thread write and read).
// ============================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#include "tray.h"

// Custom window message posted by the tray icon
#define WM_TRAY_ICON  (WM_USER + 100)
#define TRAY_UID      1

namespace TRAY
{
    std::atomic<bool> wantsShow { false };
    std::atomic<bool> wantsLock { false };
    std::atomic<bool> wantsExit { false };

    static HWND            s_hwnd  = nullptr;
    static NOTIFYICONDATAA s_nid   = {};
}

// ============================================================
// Context menu
// ============================================================
static void ShowTrayMenu(HWND hwnd)
{
    // SetForegroundWindow is required so TrackPopupMenu dismisses on click-away
    SetForegroundWindow(hwnd);

    HMENU hMenu = CreatePopupMenu();
    AppendMenuA(hMenu, MF_STRING,    1, "Open PassVault");
    AppendMenuA(hMenu, MF_STRING,    2, "Lock Vault");
    AppendMenuA(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hMenu, MF_STRING,    3, "Exit");
    SetMenuDefaultItem(hMenu, 1, FALSE);  // bold the "Open" item

    POINT pt;
    GetCursorPos(&pt);
    UINT cmd = TrackPopupMenu(hMenu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
        pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(hMenu);

    // Required so the menu doesn't stay alive in the message queue
    PostMessageA(hwnd, WM_NULL, 0, 0);

    switch (cmd)
    {
    case 1: TRAY::wantsShow.store(true); break;
    case 2: TRAY::wantsLock.store(true); break;
    case 3: TRAY::wantsExit.store(true); break;
    }
}

// ============================================================
// Subclass window procedure
// ============================================================
static LRESULT CALLBACK TraySubclassProc(
    HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
    UINT_PTR /*idSubclass*/, DWORD_PTR /*refData*/)
{
    if (msg == WM_TRAY_ICON)
    {
        switch (LOWORD(lp))
        {
        // Left click or double-click -> restore window
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            TRAY::wantsShow.store(true);
            break;

        // Right click -> show context menu
        case WM_RBUTTONUP:
            ShowTrayMenu(hwnd);
            break;
        }
        return 0;
    }

    return DefSubclassProc(hwnd, msg, wp, lp);
}

// ============================================================
// Public API
// ============================================================
void TRAY::Init(HWND hwnd)
{
    s_hwnd = hwnd;

    // Prefer the app's own embedded icon (resource ID 1); fall back to shield
    HICON hIcon = (HICON)LoadImage(
        GetModuleHandle(NULL),
        MAKEINTRESOURCE(101),     
        IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),
        GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR | LR_SHARED);
    if (!hIcon)
        hIcon = LoadIcon(NULL, IDI_SHIELD);

    ZeroMemory(&s_nid, sizeof(s_nid));
    s_nid.cbSize           = sizeof(s_nid);
    s_nid.hWnd             = hwnd;
    s_nid.uID              = TRAY_UID;
    s_nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    s_nid.uCallbackMessage = WM_TRAY_ICON;
    s_nid.hIcon            = hIcon;
    strcpy_s(s_nid.szTip, "PassVault");

    Shell_NotifyIconA(NIM_ADD, &s_nid);

    // Subclass the GLFW window to intercept WM_TRAY_ICON messages
    SetWindowSubclass(hwnd, TraySubclassProc, 1, 0);
}

void TRAY::Destroy()
{
    Shell_NotifyIconA(NIM_DELETE, &s_nid);

    if (s_hwnd)
    {
        RemoveWindowSubclass(s_hwnd, TraySubclassProc, 1);
        s_hwnd = nullptr;
    }
}

void TRAY::MinimizeToTray()
{
    if (s_hwnd) ShowWindow(s_hwnd, SW_HIDE);
}

void TRAY::RestoreWindow()
{
    if (!s_hwnd) return;
    ShowWindow(s_hwnd, SW_RESTORE);
    SetForegroundWindow(s_hwnd);
}

bool TRAY::IsHidden()
{
    return s_hwnd && !IsWindowVisible(s_hwnd);
}
