// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  updater.h  -  GitHub release check (pure logic, no UI)
//
//  Usage:
//    Call UPDATER::StartCheck() to kick off a background check.
//    Read UPDATER::state, latestVersion, downloadUrl from the
//    render thread (screens.cpp) to display results.
// ============================================================

#pragma once
#include <string>
#include <atomic>

namespace UPDATER {

    constexpr const char* CURRENT_VERSION = "v1.2.0";
    constexpr const char* RELEASES_API    =
        "https://api.github.com/repos/jemile/PassVault/releases/latest";
    constexpr const char* RELEASES_PAGE   =
        "https://github.com/jemile/PassVault/releases/latest";

    enum class State { Idle, Checking, UpToDate, Available, Downloading, ReadyToApply, Error };

    // Written by background thread before state flip; read-only on render thread.
    extern std::atomic<State> state;
    extern std::string        latestVersion;
    extern std::string        downloadUrl;
    extern std::string        errorMessage;     // readable error detail
    extern std::atomic<float> downloadProgress; // 0.0 - 1.0 while Downloading

    // Spawn the background check thread (no-op if already checking).
    void StartCheck();

    // Spawn the background download + extract thread (no-op if already running).
    void StartDownload();

    // Launch the updater script and signal the app to exit.
    // Call FRAME::shouldExit = true immediately after.
    void ApplyUpdate();

} 
