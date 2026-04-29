// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  panels.cpp  -  Vault content panel renderers
//  Contains: ClearEditBuffers, RenderSidebar, RenderDetailPanel
// ============================================================

#include "panels.h"
#include "frame.h"
#include "render.h"
#include "settings.h"
#include <cstring>
#include <algorithm>
#include <cmath>


// ============================================================
//  Internal: Draw a filled 5-pointed star via triangle fan
//  cx,cy = centre; r = outer radius; col = ARGB colour
// ============================================================
static void DrawFilledStar(ImDrawList* dl, float cx, float cy, float r, ImU32 col)
{
    const float PI     = 3.14159265358979f;
    const float inner  = r * 0.40f;
    ImVec2 pts[10];
    for (int i = 0; i < 10; ++i)
    {
        float angle = -PI * 0.5f + i * PI / 5.0f;
        float rad   = (i % 2 == 0) ? r : inner;
        pts[i] = ImVec2(cx + rad * cosf(angle), cy + rad * sinf(angle));
    }
    for (int i = 0; i < 10; ++i)
        dl->AddTriangleFilled(ImVec2(cx, cy), pts[i], pts[(i + 1) % 10], col);
}


// ============================================================
//  Internal: Populate edit buffers from an existing entry
// ============================================================
static void LoadEntryIntoBuffers(const PasswordEntry& e, UIState& s)
{
    CONVERSIONS::StrToCharBuf(e.title,    s.editTitle,    sizeof(s.editTitle));
    CONVERSIONS::StrToCharBuf(e.website,  s.editWebsite,  sizeof(s.editWebsite));
    CONVERSIONS::StrToCharBuf(e.username, s.editUsername, sizeof(s.editUsername));
    CONVERSIONS::StrToCharBuf(e.password, s.editPassword, sizeof(s.editPassword));
    CONVERSIONS::StrToCharBuf(e.notes,    s.editNotes,    sizeof(s.editNotes));

    s.editTags     = e.tags;
    s.editingId    = e.id;
    s.showPassword = false;
    memset(s.newTagBuf, 0, sizeof(s.newTagBuf));
    s.newTagColorIdx = 0;
}


// ============================================================
//  ClearEditBuffers  -  Reset all edit form fields
// ============================================================
void ClearEditBuffers(UIState& s)
{
    memset(s.editTitle,    0, sizeof(s.editTitle));
    memset(s.editWebsite,  0, sizeof(s.editWebsite));
    memset(s.editUsername, 0, sizeof(s.editUsername));
    memset(s.editPassword, 0, sizeof(s.editPassword));
    memset(s.editNotes,    0, sizeof(s.editNotes));
    memset(s.newTagBuf,    0, sizeof(s.newTagBuf));

    s.editTags.clear();
    s.editingId      = "";
    s.showPassword   = false;
    s.newTagColorIdx = 0;
}


// ============================================================
//  RenderSidebar  -  Search, filter, and entry list
// ============================================================
void RenderSidebar(const std::vector<int>& filtered, UIState& s)
{
    using namespace FRAME;

    const float W = ImGui::GetContentRegionAvail().x;

    ImGui::Spacing();

    // ---- Search ----
    RENDER::FieldLabel("SEARCH", fontSmall, theme);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, THEME::TC(
        ImVec4(0.055f, 0.063f, 0.098f, 1.0f),
        ImVec4(0.868f, 0.852f, 0.820f, 1.0f), theme));
    ImGui::SetNextItemWidth(W);
    ImGui::InputTextWithHint("##search", "Search entries...", s.searchBuf, sizeof(s.searchBuf));
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ---- Filter ----
    RENDER::FieldLabel("FILTER", fontSmall, theme);
    ImGui::Spacing();
    ImGui::SetCursorPosX(5.0f);

    // Tracks X position within the current filter row for auto-wrapping
    float filterCurX = 5.0f;

    // Advance cursor: SameLine or wrap to next row depending on button width
    auto FilterAdvance = [&](const char* label) 
    {
        float btnW = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        if (filterCurX > 5.0f) {
            if (filterCurX + 4.0f + btnW > W - 4.0f) {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
                ImGui::SetCursorPosX(5.0f);
                filterCurX = 5.0f;
            } else {
                ImGui::SameLine(0, 4);
                filterCurX += 4.0f;
            }
        }
        filterCurX += btnW;
    };

    // Render a single styled filter button (active/inactive colours + wrapping)
    auto FilterBtn = [&](const char* label, bool active,
                        ImVec4 activeCol, ImVec4 inactiveTextCol) -> bool 
    {
        FilterAdvance(label);
        ImGui::PushStyleColor(ImGuiCol_Button, active
            ? ImVec4(activeCol.x * 0.6f, activeCol.y * 0.6f, activeCol.z * 0.6f, 1.0f)
            : THEME::TC(ImVec4(0.110f, 0.125f, 0.196f, 1.0f),
                ImVec4(0.858f, 0.842f, 0.808f, 1.0f), theme));
        ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(1, 1, 1, 1) : inactiveTextCol);
        bool clicked = ImGui::Button(label);
        ImGui::PopStyleColor(2);
        return clicked;
    };

    // "All" button
    if (FilterBtn("All",
        !s.filterFavorites && s.filterTag.empty(),
        ImVec4(0.390f, 0.366f, 0.780f, 1.0f),
        THEME::TC(ImVec4(0.65f, 0.70f, 0.80f, 1.0f),
            ImVec4(0.30f, 0.34f, 0.44f, 1.0f), theme)))
    {
        s.filterTag = "";
        s.filterFavorites = false;
    }

    // "Favs" button (gold tint when inactive)
    {
        ImVec4 favCol(0.980f, 0.820f, 0.100f, 1.0f);
        if (FilterBtn("Favs", s.filterFavorites, favCol, favCol))
        {
            s.filterFavorites = !s.filterFavorites;
            if (s.filterFavorites) s.filterTag = "";
        }
    }

    // Dynamic per-tag buttons (all unique tags across all entries)
    {
        std::vector<std::string> allTags;
        for (const auto& e : s.pm.entries)
            for (const auto& t : e.tags)
                if (std::find(allTags.begin(), allTags.end(), t) == allTags.end())
                    allTags.push_back(t);

        for (size_t ti = 0; ti < allTags.size(); ++ti)
        {
            const std::string& tag = allTags[ti];
            bool               active = (!s.filterFavorites && s.filterTag == tag);
            ImVec4             col = SETTINGS::GetTagColor(tag);

            ImGui::PushID((int)(200 + ti));

            if (FilterBtn(tag.c_str(), active, col, col))
            {
                s.filterTag = (s.filterTag == tag) ? "" : tag;
                s.filterFavorites = false;
            }

            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 14));   // bigger outer padding
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 12));    // bigger vertical padding on items
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));       // more space between items

            // === RIGHT-CLICK DELETE FOR TAG ===
            if (ImGui::BeginPopupContextItem())
            {

                ImGui::PushStyleColor(ImGuiCol_PopupBg, THEME::TC(
                    ImVec4(0.082f, 0.090f, 0.137f, 1.0f),
                    ImVec4(0.970f, 0.958f, 0.934f, 1.0f), theme));

                ImGui::PushStyleColor(ImGuiCol_Header, THEME::TC(
                    ImVec4(0.25f, 0.25f, 0.40f, 1.0f),      // dark mode hover
                    ImVec4(0.70f, 0.70f, 0.85f, 1.0f), theme)); // light mode hover

                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, THEME::TC(
                    ImVec4(0.192f, 0.176f, 0.20f, 1.0f),
                    ImVec4(0.192f, 0.176f, 0.20f, 1.0f), theme));

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.00f, 0.00f, 1.0f));

                if (ImGui::MenuItem("\t\t(X) Delete Tag"))
                {
                    s.pendingDeleteTag = tag;
                    s.showDeleteTagConfirm = true;
                }

                ImGui::PopStyleColor(4);
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar(4);

            ImGui::PopID();
        }
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, THEME::TC(
        ImVec4(0.118f, 0.137f, 0.212f, 1.0f),
        ImVec4(0.712f, 0.692f, 0.648f, 1.0f), theme));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ---- Entry List ----
    float listH = ImGui::GetContentRegionAvail().y - 52.0f;
    ImGui::BeginChild("##entrylist", ImVec2(W, listH), false);

    if (filtered.empty())
    {
        ImGui::Spacing();
        const char* msg = s.filterFavorites ? "No favorites yet." : "No entries found.";
        float textW = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX((W - textW) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.40f, 0.50f, 1.0f));
        ImGui::TextUnformatted(msg);
        ImGui::PopStyleColor();
    }
    else
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        for (int idx : filtered)
        {
            const PasswordEntry& e = s.pm.entries[idx];
            bool selected = (idx == s.selectedIdx);

            ImGui::PushID(idx);

            ImVec2 screenPos = ImGui::GetCursorScreenPos();
            float  itemH = 56.0f;

            // Invisible button for row selection
            if (ImGui::InvisibleButton("##item", ImVec2(W, itemH)))
            {
                s.selectedIdx = idx;
                s.editMode = false;
                s.isNewEntry = false;
                s.showPassword = false;
            }

            bool hovered = ImGui::IsItemHovered();

            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18, 14));   // bigger outer padding
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 12));    // bigger vertical padding on items
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 8));       // more space between items

            if (ImGui::BeginPopupContextItem())
            {
                ImGui::PushStyleColor(ImGuiCol_PopupBg, THEME::TC(
                    ImVec4(0.082f, 0.090f, 0.137f, 1.0f),
                    ImVec4(0.970f, 0.958f, 0.934f, 1.0f), theme));

                ImGui::PushStyleColor(ImGuiCol_Header, THEME::TC(
                    ImVec4(0.25f, 0.25f, 0.40f, 1.0f),      // dark mode hover
                    ImVec4(0.70f, 0.70f, 0.85f, 1.0f), theme)); // light mode hover

                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, THEME::TC(
                    ImVec4(0.192f, 0.176f, 0.20f, 1.0f),
                    ImVec4(0.192f, 0.176f, 0.20f, 1.0f), theme));

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 0.00f, 0.00f, 1.0f));

                if (ImGui::MenuItem("\t\t(X) Delete Login"))
                {
                    s.pendingDeleteIdx = idx;      // remember the exact row
                    s.showDeleteConfirm = true;
                }

                ImGui::PopStyleColor(4);
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar(4);

            // Row background
            ImU32 bgCol;
            if (selected) bgCol = IM_COL32(60, 56, 122, 220);
            else if (hovered)  bgCol = THEME::TCU(IM_COL32(28, 32, 50, 200), IM_COL32(210, 198, 178, 200), theme);
            else               bgCol = IM_COL32(0, 0, 0, 0);

            dl->AddRectFilled(screenPos, ImVec2(screenPos.x + W, screenPos.y + itemH), bgCol, 6.0f);

            // Tag colour dot (first tag's colour, or slate if no tags)
            ImVec4 dotColF = e.tags.empty()
                ? ImVec4(0.58f, 0.63f, 0.73f, 1.0f)
                : SETTINGS::GetTagColor(e.tags[0]);
            ImU32 dotCol = ImGui::ColorConvertFloat4ToU32(dotColF);
            dl->AddCircleFilled(ImVec2(screenPos.x + 16, screenPos.y + itemH * 0.40f), 5.0f, dotCol, 16);

            // Title
            ImVec4 titleCol = selected
                ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                : THEME::TC(ImVec4(0.886f, 0.902f, 0.941f, 1.0f), ImVec4(0.100f, 0.105f, 0.120f, 1.0f), theme);
            ImGui::SetCursorScreenPos(ImVec2(screenPos.x + 30, screenPos.y + 9));
            ImGui::PushStyleColor(ImGuiCol_Text, titleCol);
            std::string title = e.title.empty() ? "(untitled)" : e.title;
            if (title.size() > 24) title = title.substr(0, 22) + "..";
            ImGui::TextUnformatted(title.c_str());
            ImGui::PopStyleColor();

            // Website (dimmer, small font)
            ImGui::SetCursorScreenPos(ImVec2(screenPos.x + 30, screenPos.y + 32));
            ImGui::PushFont(fontSmall);
            ImGui::PushStyleColor(ImGuiCol_Text, selected
                ? ImVec4(0.75f, 0.72f, 1.0f, 1.0f)
                : ImVec4(0.39f, 0.45f, 0.55f, 1.0f));
            std::string site = e.website;
            for (const char* pre : { "https://", "http://", "www." })
                if (site.rfind(pre, 0) == 0) { site = site.substr(strlen(pre)); break; }
            if (site.size() > 26) site = site.substr(0, 24) + "..";
            ImGui::TextUnformatted(site.c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();

            // Favorite star (right edge, gold if starred)
            if (e.isFavorite)
            {
                DrawFilledStar(dl, screenPos.x + W - 16.0f, screenPos.y + itemH * 0.40f, 6.0f, IM_COL32(255, 210, 50, 220));
            }
            else if (hovered || selected)
            {
                // Show dim outline star on hover/select
                DrawFilledStar(dl, screenPos.x + W - 16.0f, screenPos.y + itemH * 0.40f, 6.0f,
                    THEME::TCU(IM_COL32(80, 90, 110, 140), IM_COL32(160, 155, 145, 140), theme));
            }

            // Star click-to-toggle (invisible hit area over the star)
            ImGui::SetCursorScreenPos(ImVec2(screenPos.x + W - 28.0f, screenPos.y + 4.0f));
            ImGui::PushID(1000 + idx);
            if (ImGui::InvisibleButton("##star", ImVec2(24.0f, itemH - 8.0f)))
            {
                s.pm.entries[idx].isFavorite = !s.pm.entries[idx].isFavorite;
                s.pm.SaveToFile();
            }
            ImGui::PopID();

            ImGui::SetCursorScreenPos(ImVec2(screenPos.x, screenPos.y + itemH + 3));
            ImGui::PopID();
        }
    }

    ImGui::EndChild();

    // ---- Footer ----
    ImGui::PushStyleColor(ImGuiCol_Separator, THEME::TC(
        ImVec4(0.118f, 0.137f, 0.212f, 1.0f),
        ImVec4(0.712f, 0.692f, 0.648f, 1.0f), theme));
    ImGui::Separator();
    ImGui::PopStyleColor();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
    ImGui::SetCursorPosX(5);

    if (RENDER::GreenButton("  +  Add Entry  "))
    {
        ClearEditBuffers(s);
        s.editMode    = true;
        s.isNewEntry  = true;
        s.selectedIdx = -1;
    }

    ImGui::SameLine();

    ImGui::PushFont(fontSmall);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.39f, 0.45f, 0.55f, 1.0f));
    char countBuf[64];
    snprintf(countBuf, sizeof(countBuf), "%d stored", (int)s.pm.entries.size());
    float textW = ImGui::CalcTextSize(countBuf).x;
    ImGui::SetCursorPosX(W - textW - 4);
    ImGui::TextUnformatted(countBuf);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}


// ============================================================
//  RenderDetailPanel  -  Welcome state, edit form, or view mode
// ============================================================
void RenderDetailPanel(UIState& s)
{
    using namespace FRAME;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    // ---- Welcome / empty state ----
    if (s.selectedIdx < 0 && !s.editMode)
    {
        float avail = ImGui::GetContentRegionAvail().y;
        ImGui::Dummy(ImVec2(0, avail * 0.30f));

        ImGui::PushFont(fontTitle);
        float tw = ImGui::CalcTextSize("Welcome to PassVault").x;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - tw) * 0.5f);
        ImGui::TextColored(ImVec4(0.65f, 0.61f, 1.0f, 1.0f), "Welcome to PassVault");
        ImGui::PopFont();

        ImGui::Spacing();
        const char* sub = "Select an entry or press  +  Add Entry  to get started.";
        float sw = ImGui::CalcTextSize(sub).x;
        ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - sw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.39f, 0.45f, 0.55f, 1.0f));
        ImGui::TextUnformatted(sub);
        ImGui::PopStyleColor();

        ImGui::PopStyleVar();
        return;
    }

    // ---- Edit / Add form ----
    if (s.editMode)
    {
        const float W    = ImGui::GetContentRegionAvail().x;
        const float pwW  = W - 20;
        const float pwW2 = W - 140;

        ImGui::PushFont(fontTitle);
        ImGui::SetCursorPosX(5);
        ImGui::SetCursorPosY(5);
        ImGui::TextColored(ImVec4(0.65f, 0.61f, 1.0f, 1.0f),
            s.isNewEntry ? "New Entry" : "Edit Entry");
        ImGui::PopFont();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Title
        RENDER::FieldLabel("TITLE", fontSmall, theme);
        ImGui::SetCursorPosX(2);
        ImGui::SetNextItemWidth(pwW);
        ImGui::InputText("##etitle", s.editTitle, sizeof(s.editTitle));
        ImGui::Spacing();

        // Website
        RENDER::FieldLabel("WEBSITE", fontSmall, theme);
        ImGui::SetCursorPosX(2);
        ImGui::SetNextItemWidth(pwW);
        ImGui::InputText("##ewebsite", s.editWebsite, sizeof(s.editWebsite));
        ImGui::Spacing();

        // Username
        RENDER::FieldLabel("USERNAME / EMAIL", fontSmall, theme);
        ImGui::SetCursorPosX(2);
        ImGui::SetNextItemWidth(pwW);
        ImGui::InputText("##euser", s.editUsername, sizeof(s.editUsername));
        ImGui::Spacing();

        // Password row
        RENDER::FieldLabel("PASSWORD", fontSmall, theme);
        ImGui::SetCursorPosX(2);
        ImGui::SetNextItemWidth(pwW2);
        ImGuiInputTextFlags pwFlags = s.showPassword ? ImGuiInputTextFlags_None
                                                     : ImGuiInputTextFlags_Password;
        ImGui::InputText("##epw", s.editPassword, sizeof(s.editPassword), pwFlags);

        ImGui::SameLine(0, 6);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        if (ImGui::Button(s.showPassword ? " Hide " : " Show "))
            s.showPassword = !s.showPassword;

        ImGui::SameLine(0, 6);
        if (ImGui::Button("  Gen  "))
        {
            if (strlen(s.genPreview) == 0)
            {
                std::string pw = PasswordManager::GeneratePassword(
                    s.genLength, s.genUpper, s.genLower, s.genDigits, s.genSymbols);
                CONVERSIONS::StrToCharBuf(pw, s.genPreview, sizeof(s.genPreview));
            }
            s.showGenPopup = true;
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();

        // Strength bar
        if (strlen(s.editPassword) > 0)
            RENDER::DrawStrengthBar(s.editPassword, fontSmall, theme);
        ImGui::Spacing();

        // ---- Tags section ----
        RENDER::FieldLabel("TAGS", fontSmall, theme);
        ImGui::SetCursorPosX(2);

        // Existing tag chips
        ImDrawList* dlTags = ImGui::GetWindowDrawList();
        float chipX = ImGui::GetCursorPosX() + ImGui::GetWindowPos().x - ImGui::GetScrollX();
        float chipY = ImGui::GetCursorPosY() + ImGui::GetWindowPos().y - ImGui::GetScrollY();
        float chipStartX = chipX;
        const float chipH       = 20.0f;
        const float chipPadX    = 8.0f;
        const float chipSpacing = 5.0f;
        const float maxChipRowW = pwW - 4.0f;

        if (!s.editTags.empty())
        {
            float curX = chipStartX;
            float curY = chipY;
            for (size_t ti = 0; ti < s.editTags.size(); ++ti)
            {
                const std::string& tag   = s.editTags[ti];
                ImVec4             tCol  = SETTINGS::GetTagColor(tag);
                ImU32              tColU = ImGui::ColorConvertFloat4ToU32(tCol);
                ImU32              tColD = IM_COL32(
                    (int)(tCol.x * 120), (int)(tCol.y * 120), (int)(tCol.z * 120), 200);

                float labelW = ImGui::CalcTextSize(tag.c_str()).x;
                float chipW  = labelW + chipPadX * 2 + 14.0f;  // +14 for x button

                // Wrap to next row if needed
                if (curX + chipW - chipStartX > maxChipRowW && curX > chipStartX)
                {
                    curX  = chipStartX;
                    curY += chipH + 4.0f;
                }

                // Background pill
                dlTags->AddRectFilled(
                    ImVec2(curX, curY),
                    ImVec2(curX + chipW, curY + chipH),
                    tColD, 10.0f);
                dlTags->AddRect(
                    ImVec2(curX, curY),
                    ImVec2(curX + chipW, curY + chipH),
                    tColU, 10.0f, 0, 1.2f);

                // Tag label
                dlTags->AddText(
                    ImVec2(curX + chipPadX, curY + (chipH - ImGui::GetFontSize()) * 0.5f),
                    IM_COL32(255, 255, 255, 240),
                    tag.c_str());

                // x remove button (invisible button over the right side of the chip)
                ImGui::SetCursorScreenPos(ImVec2(curX + labelW + chipPadX + 1, curY + 1));
                ImGui::PushID((int)(500 + ti));
                if (ImGui::InvisibleButton("##xtag", ImVec2(14.0f, chipH - 2.0f)))
                {
                    s.editTags.erase(s.editTags.begin() + ti);
                    ImGui::PopID();
                    break;  // vector changed; safe to break since we redraw next frame
                }
                ImGui::PopID();

                // Draw x symbol
                float xCx = curX + labelW + chipPadX + 7.0f;
                float xCy = curY + chipH * 0.5f;
                dlTags->AddLine(ImVec2(xCx - 3, xCy - 3), ImVec2(xCx + 3, xCy + 3), IM_COL32(255,255,255,200), 1.4f);
                dlTags->AddLine(ImVec2(xCx + 3, xCy - 3), ImVec2(xCx - 3, xCy + 3), IM_COL32(255,255,255,200), 1.4f);

                curX += chipW + chipSpacing;
            }

            // Advance cursor below the last row of chips
            // curY is the screen Y of the last row; convert back to window-local coords
            float bottomY = curY + chipH - (ImGui::GetWindowPos().y - ImGui::GetScrollY());
            ImGui::SetCursorPosY(bottomY + 6.0f);
        }

        // Add-tag row: [input] [Add]
        ImGui::SetCursorPosX(2);
        ImGui::SetNextItemWidth(pwW - 68.0f);
        bool enterPressed = false;
        if (ImGui::InputText("##newtagname", s.newTagBuf, sizeof(s.newTagBuf),
            ImGuiInputTextFlags_EnterReturnsTrue))
            enterPressed = true;

        ImGui::SameLine(0, 6);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
        bool addPressed = ImGui::Button(" + Add ");
        ImGui::PopStyleColor();

        if ((addPressed || enterPressed) && s.newTagBuf[0] != '\0')
        {
            std::string newTag = s.newTagBuf;
            // Only add if not a duplicate
            if (std::find(s.editTags.begin(), s.editTags.end(), newTag) == s.editTags.end())
            {
                s.editTags.push_back(newTag);
                // Assign colour if not already known
                if (SETTINGS::tagColorMap.find(newTag) == SETTINGS::tagColorMap.end())
                    SETTINGS::SetTagColor(newTag, s.newTagColorIdx);
            }
            memset(s.newTagBuf, 0, sizeof(s.newTagBuf));
        }

        {
            // Collect all known tags not already applied to this entry
            std::vector<std::string> available;
            for (const auto& entry : s.pm.entries)
                for (const auto& t : entry.tags)
                    if (std::find(s.editTags.begin(), s.editTags.end(), t) == s.editTags.end()
                     && std::find(available.begin(), available.end(), t) == available.end())
                        available.push_back(t);
            for (const auto& kv : SETTINGS::tagColorMap)
                if (std::find(s.editTags.begin(), s.editTags.end(), kv.first) == s.editTags.end()
                 && std::find(available.begin(), available.end(), kv.first) == available.end())
                    available.push_back(kv.first);

            if (!available.empty())
            {
                ImGui::SetCursorPosX(2);
                ImGui::PushFont(fontSmall);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.50f, 0.60f, 1.0f));
                ImGui::TextUnformatted("Existing:");
                ImGui::PopStyleColor();
                ImGui::PopFont();

                // -- Rebuild available tags EVERY frame (this fixes the stale list) --
                std::vector<std::string> available;
                for (const auto& e : s.pm.entries)
                {
                    for (const auto& t : e.tags)
                    {
                        // Only show tags that are NOT already in the current entry being edited
                        if (std::find(s.editTags.begin(), s.editTags.end(), t) == s.editTags.end() &&
                            std::find(available.begin(), available.end(), t) == available.end())
                        {
                            available.push_back(t);
                        }
                    }
                }

                float existCurX = 2.0f;
                ImGui::SetCursorPosX(2.0f);

                for (size_t ai = 0; ai < available.size(); ++ai)
                {
                    const std::string& tag = available[ai];
                    ImVec4             col = SETTINGS::GetTagColor(tag);
                    float              btnW = ImGui::CalcTextSize(tag.c_str()).x
                        + ImGui::GetStyle().FramePadding.x * 2.0f;

                    if (existCurX > 2.0f) {
                        if (existCurX + 4.0f + btnW > pwW - 4.0f) {
                            ImGui::NewLine();
                            ImGui::SetCursorPosX(2.0f);
                            existCurX = 2.0f;
                        }
                        else {
                            ImGui::SameLine(0, 4);
                            existCurX += 4.0f;
                        }
                    }
                    existCurX += btnW;

                    ImGui::PushID((int)(800 + ai));
                    ImGui::PushStyleColor(ImGuiCol_Button,
                        THEME::TC(ImVec4(0.095f, 0.110f, 0.175f, 1.0f),
                            ImVec4(0.848f, 0.832f, 0.798f, 1.0f), theme));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        THEME::TC(ImVec4(0.130f, 0.148f, 0.235f, 1.0f),
                            ImVec4(0.800f, 0.782f, 0.748f, 1.0f), theme));
                    ImGui::PushStyleColor(ImGuiCol_Text, col);

                    if (ImGui::Button(tag.c_str()))
                        s.editTags.push_back(tag);

                    ImGui::PopStyleColor(3);
                    ImGui::PopID();
                }
                ImGui::Spacing();
            }
        }

        // Colour palette row
        ImGui::SetCursorPosX(2);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);

        ImGui::PushFont(fontSmall);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.45f, 0.50f, 0.60f, 1.0f));
        ImGui::TextUnformatted("Color:");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::SameLine(0, 6);
        {
            ImDrawList* dlPal = ImGui::GetWindowDrawList();
            float       px    = ImGui::GetCursorScreenPos().x + 5.0f;
            float       py    = ImGui::GetCursorScreenPos().y + 2.0f;
            const float dotR  = 8.0f;
            const float dotGap = 19.5f;

            for (int pi = 0; pi < SETTINGS::TAG_PALETTE_SIZE; ++pi)
            {
                const float* c = SETTINGS::TAG_PALETTE[pi];
                ImU32 palCol   = IM_COL32(
                    (int)(c[0]*255), (int)(c[1]*255), (int)(c[2]*255), 255);
                bool  selPal   = (s.newTagColorIdx == pi);

                ImGui::SetCursorScreenPos(ImVec2(px + pi * dotGap - dotR, py - dotR));
                ImGui::PushID(700 + pi);
                if (ImGui::InvisibleButton("##palDot", ImVec2(dotR * 2, dotR * 2)))
                    s.newTagColorIdx = pi;
                ImGui::PopID();

                dlPal->AddCircleFilled(ImVec2(px + pi * dotGap, py + dotR - 1), dotR, palCol, 16);
                if (selPal)
                    dlPal->AddCircle(ImVec2(px + pi * dotGap, py + dotR - 1), dotR + 2.5f,
                        IM_COL32(255,255,255,220), 16, 1.8f);
            }

            // Advance cursor past the palette dots
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + dotR * 2 + 6.0f);
        }

        ImGui::Spacing();

        // Notes
        RENDER::FieldLabel("NOTES", fontSmall, theme);
        ImGui::SetCursorPosX(2);
        ImGui::InputTextMultiline("##enotes", s.editNotes, sizeof(s.editNotes),
            ImVec2(pwW, 90));
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Action buttons
        ImGui::SetCursorPosX(5);
        if (RENDER::GreenButton(s.isNewEntry ? "  Save Entry  " : "  Save Changes  "))
        {
            PasswordEntry e{};
            e.id       = s.editingId;
            e.title    = s.editTitle;
            e.website  = s.editWebsite;
            e.username = s.editUsername;
            e.password = s.editPassword;
            e.tags     = s.editTags;
            e.notes    = s.editNotes;

            if (s.isNewEntry)
            {
                s.pm.AddEntry(e);
                s.selectedIdx = (int)s.pm.entries.size() - 1;
                RENDER::ShowToast("Entry saved!", s.toastMsg, s.toastTimer);
            }
            else
            {
                // Preserve isFavorite from the original entry
                int origIdx = s.pm.FindIndexById(e.id);
                if (origIdx >= 0) e.isFavorite = s.pm.entries[origIdx].isFavorite;
                s.pm.UpdateEntry(e);
                s.selectedIdx = s.pm.FindIndexById(e.id);
                RENDER::ShowToast("Changes saved!", s.toastMsg, s.toastTimer);
            }

            s.editMode   = false;
            s.isNewEntry = false;
        }

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        if (ImGui::Button("  Cancel  "))
        {
            s.editMode   = false;
            s.isNewEntry = false;
        }
        ImGui::PopStyleColor();

        ImGui::PopStyleVar();
        return;
    }

    // ---- View mode ----
    if (s.selectedIdx < 0 || s.selectedIdx >= (int)s.pm.entries.size())
    {
        ImGui::PopStyleVar();
        return;
    }

    const PasswordEntry& e  = s.pm.entries[s.selectedIdx];
    const float W   = ImGui::GetContentRegionAvail().x;
    const float pwW = W - 20;

    // Header: title + favorite toggle
    ImGui::SetCursorPosX(5);
    ImGui::SetCursorPosY(6);
    ImGui::PushFont(fontTitle);
    ImGui::TextColored(THEME::TC(
        ImVec4(0.886f, 0.902f, 0.941f, 1.0f),
        ImVec4(0.100f, 0.105f, 0.120f, 1.0f), theme),
        e.title.empty() ? "(untitled)" : e.title.c_str());
    ImGui::PopFont();

    // Favorite toggle (inline star button)
    ImGui::SameLine(0, 10);
    {
        bool fav = e.isFavorite;
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f,0.3f,0.1f,0.3f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.4f,0.4f,0.1f,0.4f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            fav ? ImVec4(1.0f, 0.82f, 0.10f, 1.0f)   // gold
                : ImVec4(0.45f, 0.50f, 0.60f, 1.0f)); // dim
        ImGui::SetCursorPosX(W - 50);
        ImGui::SetCursorPosY(6);
        if (ImGui::Button(fav ? "Fav" : "Fav", ImVec2(0, 0)))
        {
            s.pm.entries[s.selectedIdx].isFavorite = !fav;
            s.pm.SaveToFile();
        }
        ImGui::PopStyleColor(4);

        // Draw a small star next to the button text
        ImDrawList* dlStar = ImGui::GetWindowDrawList();
        ImVec2 btnMin = ImGui::GetItemRectMin();
        ImVec2 btnMax = ImGui::GetItemRectMax();
        float  sCx = btnMin.x - 10.0f;
        float  sCy = (btnMin.y + btnMax.y) * 0.5f;
        ImU32  sCol = fav ? IM_COL32(255,210,50,240) : IM_COL32(100,110,130,160);
        DrawFilledStar(dlStar, sCx, sCy, 7.0f, sCol);
    }

    // Tag chips (view mode)
    if (!e.tags.empty())
    {
        ImGui::SameLine(0, 14);
        ImGui::SetCursorPosY(9);
        ImGui::PushFont(fontSmall);
        for (size_t ti = 0; ti < e.tags.size(); ++ti)
        {
            if (ti > 0) ImGui::SameLine(0, 5);
            ImVec4 tCol = SETTINGS::GetTagColor(e.tags[ti]);
            ImGui::PushStyleColor(ImGuiCol_Text, tCol);
            ImGui::TextUnformatted(e.tags[ti].c_str());
            ImGui::PopStyleColor();
        }
        ImGui::PopFont();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Website
    if (!e.website.empty())
    {
        RENDER::FieldLabel("WEBSITE", fontSmall, theme);
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(0.45f, 0.72f, 1.0f, 1.0f), "%s", e.website.c_str());
        ImGui::Spacing();
    }

    // Username
    RENDER::FieldLabel("USERNAME / EMAIL", fontSmall, theme);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, THEME::TC(
        ImVec4(0.055f, 0.063f, 0.098f, 1.0f),
        ImVec4(0.868f, 0.852f, 0.820f, 1.0f), theme));
    ImGui::SetCursorPosX(2);
    ImGui::SetNextItemWidth(W - 75);
    ImGui::InputText("##vuser", (char*)e.username.c_str(), e.username.size() + 1,
        ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 8);
    RENDER::CopyButton("cpyuser", e.username.c_str(), theme, s.toastMsg, s.toastTimer);
    ImGui::Spacing();

    // Password
    RENDER::FieldLabel("PASSWORD", fontSmall, theme);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, THEME::TC(
        ImVec4(0.055f, 0.063f, 0.098f, 1.0f),
        ImVec4(0.868f, 0.852f, 0.820f, 1.0f), theme));
    ImGuiInputTextFlags vPwFlags = ImGuiInputTextFlags_ReadOnly;
    if (!s.showPassword) vPwFlags |= ImGuiInputTextFlags_Password;
    ImGui::SetCursorPosX(2);
    ImGui::SetNextItemWidth(W - 135);
    ImGui::InputText("##vpw", (char*)e.password.c_str(), e.password.size() + 1, vPwFlags);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 6);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if (ImGui::Button(s.showPassword ? " Hide " : " Show "))
        s.showPassword = !s.showPassword;
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 6);
    RENDER::CopyButton("cpypw", e.password.c_str(), theme, s.toastMsg, s.toastTimer);

    // Strength bar
    ImGui::Spacing();
    if (!e.password.empty())
    {
        RENDER::FieldLabel("STRENGTH", fontSmall, theme);
        RENDER::DrawStrengthBar(e.password.c_str(), fontSmall, theme);
    }
    ImGui::Spacing();

    // Notes
    if (!e.notes.empty())
    {
        RENDER::FieldLabel("NOTES", fontSmall, theme);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, THEME::TC(
            ImVec4(0.055f, 0.063f, 0.098f, 1.0f),
            ImVec4(0.868f, 0.852f, 0.820f, 1.0f), theme));
        ImGui::SetCursorPosX(2);
        ImGui::InputTextMultiline("##vnotes", (char*)e.notes.c_str(), e.notes.size() + 1,
            ImVec2(pwW, 80), ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // Timestamps
    ImGui::PushFont(fontSmall);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.40f, 0.50f, 1.0f));
    ImGui::SetCursorPosX(2);
    ImGui::Text("Added:     %s", e.createdAt.c_str());
    ImGui::SetCursorPosX(2);
    ImGui::Text("Modified: %s",  e.modifiedAt.c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Action buttons row
    ImGui::SetCursorPosX(2);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if (ImGui::Button("  Edit Entry  "))
    {
        LoadEntryIntoBuffers(e, s);
        s.editMode   = true;
        s.isNewEntry = false;
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if (RENDER::RedButton("  Delete  "))
        s.showDeleteConfirm = true;

    // "View History" button - only when history is available
    if (!e.passwordHistory.empty())
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,
            THEME::TC(ImVec4(0.110f, 0.125f, 0.196f, 1.0f),
                      ImVec4(0.838f, 0.822f, 0.792f, 1.0f), theme));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            THEME::TC(ImVec4(0.160f, 0.180f, 0.270f, 1.0f),
                      ImVec4(0.790f, 0.772f, 0.740f, 1.0f), theme));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            THEME::TC(ImVec4(0.090f, 0.100f, 0.160f, 1.0f),
                      ImVec4(0.730f, 0.714f, 0.684f, 1.0f), theme));
        ImGui::PushStyleColor(ImGuiCol_Text,
            THEME::TC(ImVec4(0.65f, 0.70f, 0.90f, 1.0f),
                      ImVec4(0.25f, 0.28f, 0.38f, 1.0f), theme));


        char histLabel[40];
        snprintf(histLabel, sizeof(histLabel), "  History (%d)  ", (int)e.passwordHistory.size());
        if (ImGui::Button(histLabel))
            s.showHistoryPopup = true;
        ImGui::PopStyleColor(4);
    }

    ImGui::PopStyleVar();
}