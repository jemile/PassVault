// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  render.h  –  Reusable ImGui helpers, theme utilities,
//               and string conversion declarations
//
//  Only tiny one-liners are kept inline here.  All function
//  bodies live in render.cpp.
// ============================================================

#pragma once
#include <string>
#include "ImGui/imgui.h"
#include "password_manager.h"


namespace THEME
{
    // ============================================================
    //  Inline helpers – pick dark or light colour at runtime
    //  (kept inline because they are called on every widget draw)
    // ============================================================
    inline ImVec4 TC(ImVec4 dark, ImVec4 light, int theme)
    {
        return theme == 0 ? dark : light;
    }

    inline ImU32 TCU(ImU32 dark, ImU32 light, int theme)
    {
        return theme == 0 ? dark : light;
    }

    // Inline strength-bar colour map (small switch, called per bar)
    inline ImVec4 StrengthColor(int s)
    {
        switch (s)
        {
        case 0:  return ImVec4(0.86f, 0.20f, 0.20f, 1.0f);  // red
        case 1:  return ImVec4(0.96f, 0.45f, 0.10f, 1.0f);  // orange
        case 2:  return ImVec4(0.96f, 0.80f, 0.10f, 1.0f);  // yellow
        case 3:  return ImVec4(0.56f, 0.90f, 0.30f, 1.0f);  // lime
        case 4:  return ImVec4(0.20f, 0.80f, 0.40f, 1.0f);  // green
        default: return ImVec4(0.10f, 0.95f, 0.55f, 1.0f);  // teal
        }
    }

    // Full theme colour tables – defined in render.cpp
    void DarkTheme();
    void LightTheme();
}


namespace CONVERSIONS
{
    // Copy a std::string into a fixed-size char buffer (safe truncation)
    void StrToCharBuf(const std::string& src, char* dst, size_t dstSize);
}


namespace RENDER
{
    // Show a timed toast message in the title bar
    void ShowToast(const char* msg, char toastMsg[128], float& toastTimer);

    // Horizontally center the next item using the given label width
    void CenterSpacing(const char* label);

    // Small dimmed section-label heading
    void FieldLabel(const char* txt, ImFont* font, int theme);

    // Accent-coloured push buttons
    bool GreenButton (const char* label);
    bool PurpleButton(const char* label, const ImVec2& size_arg);
    bool RedButton   (const char* label);

    // Clipboard copy button with built-in toast feedback
    bool CopyButton(const char* id, const char* textToCopy, int theme,
                    char toastMsg[128], float& toastTimer);

    // Inline password-strength progress bar + label
    void DrawStrengthBar(const char* pwd, ImFont* font, int theme);
}
