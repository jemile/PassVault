// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  panels.cpp  –  Vault content panel renderers
//  Contains: ClearEditBuffers, LoadEntryIntoBuffers,
//            RenderSidebar, RenderDetailPanel
// ============================================================

#include "panels.h"
#include "frame.h"
#include "render.h"
#include <cstring>
#include <algorithm>


// ============================================================
//  Internal: Map a category name to its accent colour
// ============================================================
static ImVec4 GetCatColor(const std::string& cat)
{
    if (cat == "Personal") return ImVec4(0.310f, 0.760f, 0.970f, 1.0f);  // sky blue
    if (cat == "Work")     return ImVec4(0.980f, 0.750f, 0.140f, 1.0f);  // amber
    if (cat == "Finance")  return ImVec4(0.130f, 0.770f, 0.370f, 1.0f);  // green
    if (cat == "Social")   return ImVec4(0.960f, 0.280f, 0.700f, 1.0f);  // pink
    return                        ImVec4(0.580f, 0.630f, 0.730f, 1.0f);  // slate (Other)
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

    s.editCatIdx = 0;
    for (int i = 0; i < NUM_CATS; ++i)
        if (e.category == CATEGORIES[i]) { s.editCatIdx = i; break; }

    s.editingId    = e.id;
    s.showPassword = false;
}


// ============================================================
//  ClearEditBuffers  –  Reset all edit form fields
// ============================================================
void ClearEditBuffers(UIState& s)
{
    memset(s.editTitle,    0, sizeof(s.editTitle));
    memset(s.editWebsite,  0, sizeof(s.editWebsite));
    memset(s.editUsername, 0, sizeof(s.editUsername));
    memset(s.editPassword, 0, sizeof(s.editPassword));
    memset(s.editNotes,    0, sizeof(s.editNotes));

    s.editCatIdx   = 0;
    s.editingId    = "";
    s.showPassword = false;
}


// ============================================================
//  RenderSidebar  –  Search, category filter, and entry list
// ============================================================
void RenderSidebar(const std::vector<int>& filtered, UIState& s)
{
    using namespace FRAME;

    const float W = ImGui::GetContentRegionAvail().x;

    ImGui::Spacing();

    // --- Search ---
    RENDER::FieldLabel("SEARCH", fontSmall, theme);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, THEME::TC(
        ImVec4(0.055f, 0.063f, 0.098f, 1.0f),
        ImVec4(0.868f, 0.852f, 0.820f, 1.0f), theme));
    ImGui::SetNextItemWidth(W);
    ImGui::InputTextWithHint("##search", "Search entries...", s.searchBuf, sizeof(s.searchBuf));
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // --- Category Filter ---
    RENDER::FieldLabel("FILTER BY CATEGORY", fontSmall, theme);
    ImGui::Spacing();
    ImGui::SetCursorPosX(5);

    // "All" button
    {
        bool active = (s.filterCatIdx == -1);
        ImGui::PushStyleColor(ImGuiCol_Button, active
            ? ImVec4(0.235f, 0.220f, 0.470f, 1.0f)
            : THEME::TC(ImVec4(0.110f, 0.125f, 0.196f, 1.0f), ImVec4(0.858f, 0.842f, 0.808f, 1.0f), theme));
        ImGui::PushStyleColor(ImGuiCol_Text, active
            ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
            : THEME::TC(ImVec4(0.65f, 0.70f, 0.80f, 1.0f), ImVec4(0.30f, 0.34f, 0.44f, 1.0f), theme));
        if (ImGui::Button("All")) s.filterCatIdx = -1;
        ImGui::PopStyleColor(2);
    }

    // Per-category buttons
    for (int i = 0; i < NUM_CATS; ++i)
    {
        ImGui::SameLine(0, 4);
        bool   active = (s.filterCatIdx == i);
        ImVec4 col    = GetCatColor(CATEGORIES[i]);

        ImGui::PushStyleColor(ImGuiCol_Button, active
            ? ImVec4(col.x * 0.6f, col.y * 0.6f, col.z * 0.6f, 1.0f)
            : THEME::TC(ImVec4(0.110f, 0.125f, 0.196f, 1.0f), ImVec4(0.858f, 0.842f, 0.808f, 1.0f), theme));
        ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f) : col);
        ImGui::PushID(i + 100);
        if (ImGui::Button(CATEGORIES[i]))
            s.filterCatIdx = (s.filterCatIdx == i) ? -1 : i;
        ImGui::PopID();
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, THEME::TC(
        ImVec4(0.118f, 0.137f, 0.212f, 1.0f),
        ImVec4(0.712f, 0.692f, 0.648f, 1.0f), theme));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // --- Entry List ---
    float listH = ImGui::GetContentRegionAvail().y - 52.0f;  // leave room for footer
    ImGui::BeginChild("##entrylist", ImVec2(W, listH), false);

    if (filtered.empty())
    {
        ImGui::Spacing();
        float textW = ImGui::CalcTextSize("No entries found.").x;
        ImGui::SetCursorPosX((W - textW) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.40f, 0.50f, 1.0f));
        ImGui::TextUnformatted("No entries found.");
        ImGui::PopStyleColor();
    }
    else
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        for (int idx : filtered)
        {
            const PasswordEntry& e        = s.pm.entries[idx];
            bool                 selected = (idx == s.selectedIdx);

            ImGui::PushID(idx);

            ImVec2 screenPos = ImGui::GetCursorScreenPos();
            float  itemH     = 56.0f;

            // Invisible button for interaction
            if (ImGui::InvisibleButton("##item", ImVec2(W, itemH)))
            {
                s.selectedIdx  = idx;
                s.editMode     = false;
                s.isNewEntry   = false;
                s.showPassword = false;
            }

            bool hovered = ImGui::IsItemHovered();

            // Row background
            ImU32 bgCol;
            if      (selected) bgCol = IM_COL32(60, 56, 122, 220);
            else if (hovered)  bgCol = THEME::TCU(IM_COL32(28, 32, 50, 200), IM_COL32(210, 198, 178, 200), theme);
            else               bgCol = IM_COL32(0, 0, 0, 0);

            dl->AddRectFilled(screenPos,
                ImVec2(screenPos.x + W, screenPos.y + itemH), bgCol, 6.0f);

            // Category colour dot
            ImU32 catU = ImGui::ColorConvertFloat4ToU32(GetCatColor(e.category));
            dl->AddCircleFilled(
                ImVec2(screenPos.x + 16, screenPos.y + itemH * 0.40f),
                5.0f, catU, 16);

            // Title
            ImVec4 titleCol = selected
                ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                : THEME::TC(ImVec4(0.886f, 0.902f, 0.941f, 1.0f), ImVec4(0.100f, 0.105f, 0.120f, 1.0f), theme);
            ImGui::SetCursorScreenPos(ImVec2(screenPos.x + 30, screenPos.y + 9));
            ImGui::PushStyleColor(ImGuiCol_Text, titleCol);
            std::string title = e.title.empty() ? "(untitled)" : e.title;
            if (title.size() > 26) title = title.substr(0, 24) + "..";
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
            if (site.size() > 28) site = site.substr(0, 26) + "..";
            ImGui::TextUnformatted(site.c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();

            // Advance cursor past item + small gap
            ImGui::SetCursorScreenPos(ImVec2(screenPos.x, screenPos.y + itemH + 3));
            ImGui::PopID();
        }
    }

    ImGui::EndChild();

    // --- Footer ---
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

    // Entry count (right-aligned)
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
//  RenderDetailPanel  –  Welcome state, edit form, or view mode
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

        // Category
        RENDER::FieldLabel("CATEGORY", fontSmall, theme);
        ImGui::SetCursorPosX(2);
        ImGui::SetNextItemWidth(pwW);
        ImGui::Combo("##ecat", &s.editCatIdx, CATEGORIES, NUM_CATS);
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
            e.category = CATEGORIES[s.editCatIdx];
            e.notes    = s.editNotes;

            if (s.isNewEntry)
            {
                s.pm.AddEntry(e);
                s.selectedIdx = (int)s.pm.entries.size() - 1;
                RENDER::ShowToast("Entry saved!", s.toastMsg, s.toastTimer);
            }
            else
            {
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
            if (s.isNewEntry) s.selectedIdx = -1;
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

    // Header: title + category badge
    ImGui::PushFont(fontTitle);
    ImGui::SetCursorPosX(5);
    ImGui::SetCursorPosY(6);
    ImGui::TextColored(THEME::TC(
        ImVec4(0.886f, 0.902f, 0.941f, 1.0f),
        ImVec4(0.100f, 0.105f, 0.120f, 1.0f), theme),
        e.title.empty() ? "(untitled)" : e.title.c_str());
    ImGui::PopFont();

    // Category badge
    ImVec4 catCol = GetCatColor(e.category);
    ImGui::SameLine(0, 12);
    ImGui::PushStyleColor(ImGuiCol_Text, catCol);
    ImGui::PushFont(fontSmall);
    ImGui::SetCursorPosY(6);
    ImGui::TextUnformatted(e.category.empty() ? "Other" : e.category.c_str());
    ImGui::PopFont();
    ImGui::PopStyleColor();

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

    // Action buttons
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

    ImGui::PopStyleVar();
}
