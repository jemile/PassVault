// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  settings.h  -  Persistent user preferences + tag colour system
//
//  Provides:
//    • A 10-swatch palette for tag colours
//    • A tag-name → palette-index registry  (saved to disk)
//    • Load() / Save() for data/settings.ini
//
//  Callers receive saved theme / autolock values through the
//  savedTheme / savedAutoLockIdx externs after calling Load().
// ============================================================

#pragma once
#include <string>
#include <map>

struct ImVec4;   // forward-declared so we don't drag in imgui.h

namespace SETTINGS
{
    // --------------------------------------------------------
    //  Tag-colour palette  (10 swatches, indices 0-9)
    //  Format: { R, G, B, A } floats
    // --------------------------------------------------------
    extern const float TAG_PALETTE[10][4];
    static const int   TAG_PALETTE_SIZE = 10;

    // Swatch names shown in the colour picker UI
    extern const char* TAG_PALETTE_NAMES[10];

    // --------------------------------------------------------
    //  Tag name → palette index  (persisted via settings.ini)
    //  Default entries cover the legacy category names.
    // --------------------------------------------------------
    extern std::map<std::string, int> tagColorMap;

    // Returns the ImVec4 colour for a tag name.
    // Falls back to palette[0] (Sky Blue) for unknown tags.
    ImVec4 GetTagColor(const std::string& tagName);

    // Assigns a palette index to a tag name.
    void   SetTagColor(const std::string& tagName, int paletteIdx);

    // --------------------------------------------------------
    //  Values populated by Load() and consumed by frame.cpp
    // --------------------------------------------------------
    extern int savedTheme;        // 0 = dark, 1 = light
    extern int savedAutoLockIdx;  // 0-4 matching the combo options

    // --------------------------------------------------------
    //  Persistence  (uses ./data/settings.ini)
    // --------------------------------------------------------
    void Load();                            // reads file → sets savedTheme, savedAutoLockIdx, tagColorMap
    void Save(int theme, int autoLockIdx);  // writes file with current state
}
