// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  toast.h  -  Toast notification types shared between
//              app_state.h (storage) and render.h (drawing)
// ============================================================

#pragma once

// Severity of the notification - controls icon and colour
enum class ToastType : int
{
    Success = 0,  // green  checkmark
    Error   = 1,  // red    X
    Info    = 2,  // blue   i
    Warning = 3   // amber  !
};

// One live toast
struct ToastEntry
{
    char      msg[128] = {};
    ToastType type     = ToastType::Success;
    float     timer    = 0.0f;  // counts down from 4.0; expired when <= 0
};
