// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  tray.h  -  System tray icon and minimize-to-tray support
//
//  Call TRAY::Init(hwnd)  after the GLFW window is created.
//  Call TRAY::Destroy()   in UnloadFrame before window close.
//
//  Each frame, check the three atomic flags and act on them:
//    wantsShow  -> TRAY::RestoreWindow()
//    wantsLock  -> lock vault, then TRAY::RestoreWindow()
//    wantsExit  -> FRAME::shouldExit = true
// ============================================================

#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <atomic>

namespace TRAY
{
    // Set by the Win32 message handler; read+cleared each frame on the main thread.
    extern std::atomic<bool> wantsShow;
    extern std::atomic<bool> wantsLock;
    extern std::atomic<bool> wantsExit;

    // Register the tray icon and subclass the GLFW HWND to receive tray messages.
    void Init(HWND hwnd);

    // Remove the tray icon and the window subclass.  Call before destroying the window.
    void Destroy();

    // Hide the window (minimize to tray).
    void MinimizeToTray();

    // Show and bring the window back to the foreground.
    void RestoreWindow();

    // Returns true while the window is hidden to the tray.
    bool IsHidden();
}
