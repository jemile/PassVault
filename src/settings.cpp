// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  settings.cpp  -  Persistent user preferences implementation
// ============================================================

#include "settings.h"
#include <ImGui/imgui.h>
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace SETTINGS
{
    // ============================================================
    //  Palette definition  (R, G, B, A)
    // ============================================================
    const float TAG_PALETTE[10][4] =
    {
        { 0.310f, 0.760f, 0.970f, 1.0f },  // 0  Sky Blue
        { 0.980f, 0.750f, 0.140f, 1.0f },  // 1  Amber
        { 0.130f, 0.770f, 0.370f, 1.0f },  // 2  Green
        { 0.960f, 0.280f, 0.700f, 1.0f },  // 3  Pink
        { 0.580f, 0.630f, 0.730f, 1.0f },  // 4  Slate
        { 0.620f, 0.380f, 0.980f, 1.0f },  // 5  Purple
        { 0.980f, 0.420f, 0.100f, 1.0f },  // 6  Orange
        { 0.950f, 0.280f, 0.280f, 1.0f },  // 7  Red
        { 0.100f, 0.780f, 0.680f, 1.0f },  // 8  Teal
        { 0.700f, 0.700f, 0.950f, 1.0f },  // 9  Lavender
    };

    // ============================================================
    //  Tag colour registry  -  pre-seeded with legacy categories
    // ============================================================
    std::map<std::string, int> tagColorMap =
    {
        { "Personal", 0 },
        { "Work",     1 },
        { "Finance",  2 },
        { "Social",   3 },
        { "Other",    4 },
    };

    // ============================================================
    //  Values loaded from disk
    // ============================================================
    int   savedTheme         = 0;
    int   savedAutoLockIdx   = 1;      // default: 1 minute
    bool  savedToastsEnabled = true;
    float savedToastDuration = 4.0f;   // seconds


    // ============================================================
    //  Internal helpers
    // ============================================================
    static std::string GetSettingsPath()
    {
        auto dataDir = std::filesystem::current_path() / "data";
        if (!std::filesystem::exists(dataDir))
            std::filesystem::create_directories(dataDir);
        return (dataDir / "settings.ini").string();
    }


    // ============================================================
    //  GetTagColor  -  returns ImVec4 for a named tag
    // ============================================================
    ImVec4 GetTagColor(const std::string& tagName)
    {
        auto it  = tagColorMap.find(tagName);
        int  idx = (it != tagColorMap.end()) ? it->second : 0;
        idx      = std::max(0, std::min(idx, TAG_PALETTE_SIZE - 1));
        const float* c = TAG_PALETTE[idx];
        return ImVec4(c[0], c[1], c[2], c[3]);
    }

    void SetTagColor(const std::string& tagName, int paletteIdx)
    {
        if (tagName.empty()) return;
        tagColorMap[tagName] = std::max(0, std::min(paletteIdx, TAG_PALETTE_SIZE - 1));
    }


    // ============================================================
    //  Load  -  reads ./data/settings.ini
    //  Populates savedTheme, savedAutoLockIdx, tagColorMap.
    // ============================================================
    void Load()
    {
        std::ifstream file(GetSettingsPath());
        if (!file.is_open()) return;  // first run - use defaults

        std::string line;
        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;

            size_t sep = line.find('=');
            if (sep == std::string::npos) continue;

            std::string key = line.substr(0, sep);
            std::string val = line.substr(sep + 1);

            try
            {
                if      (key == "theme")                savedTheme         = std::stoi(val);
                else if (key == "autolock")             savedAutoLockIdx   = std::stoi(val);
                else if (key == "toasts_enabled")       savedToastsEnabled = (std::stoi(val) != 0);
                else if (key == "toast_duration")       savedToastDuration = std::stof(val);
                else if (key.rfind("tagcolor_", 0) == 0)
                    tagColorMap[key.substr(9)] = std::stoi(val);
            }
            catch (...) {}
        }

        // Clamp to valid ranges
        savedTheme         = std::max(0, std::min(savedTheme,         1));
        savedAutoLockIdx   = std::max(0, std::min(savedAutoLockIdx,   4));
        savedToastDuration = std::max(2.0f, std::min(savedToastDuration, 8.0f));
    }


    // ============================================================
    //  Save  -  writes ./data/settings.ini
    // ============================================================
    void Save(int theme, int autoLockIdx, bool toastsEnabled, float toastDuration)
    {
        std::ofstream file(GetSettingsPath());
        if (!file.is_open()) return;

        file << "# PassVault Settings\n";
        file << "theme="           << theme             << "\n";
        file << "autolock="        << autoLockIdx       << "\n";
        file << "toasts_enabled="  << (toastsEnabled ? 1 : 0) << "\n";
        file << "toast_duration="  << toastDuration     << "\n";

        for (const auto& [name, idx] : tagColorMap)
            file << "tagcolor_" << name << "=" << idx << "\n";
    }
}
