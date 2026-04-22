// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  screens.h  –  Full-screen overlay renderers
// ============================================================

#pragma once
#include "app_state.h"

// Initial setup / unlock screen (replaces vault content when locked)
void RenderLockScreen(UIState& s);

// Settings card (overlays vault content)
void RenderSettingsScreen(UIState& s);
