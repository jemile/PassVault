// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  frame.h  –  Window state, font handles, and public entry points
//
//  Included by every render translation unit so they can reach
//  FRAME:: display globals (width, height, theme, fonts, etc.)
//  and by main.cpp for the two public lifecycle functions.
// ============================================================

#pragma once
#include "app_state.h"

#include "glad.h"
#include <GLFW/glfw3.h>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_opengl3.h>


namespace FRAME
{
    // ============================================================
    //  Window & Display State
    // ============================================================
    extern int          width;
    extern int          height;
    extern int          theme;
    extern int          filterTheme;    // -1 forces theme apply on first frame
    extern bool         shouldExit;
    extern bool         settingsTab;

    // ============================================================
    //  Auto-Lock Settings
    // ============================================================
    extern int          autoLockIndex;
    extern float        autoLockTimeout;

    // ============================================================
    //  OpenGL / ImGui Handles
    // ============================================================
    extern GLFWwindow*  window;
    extern ImFont*      font;           // regular  (~20 px)
    extern ImFont*      fontTitle;      // headings (~25 px)
    extern ImFont*      fontSmall;      // captions (~16 px)
    extern ImFont*      sunMoonFontBig; // icons    (~32 px)

    // ============================================================
    //  Public Entry Points
    // ============================================================
    void SetupFrame();
    void UnloadFrame();
}
