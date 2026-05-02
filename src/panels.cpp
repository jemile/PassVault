// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  panels.cpp  -  Sidebar and shared panel helpers
//  Contains: ClearEditBuffers, RenderSidebar
//  See detail_panel.cpp for RenderDetailPanel
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
void DrawFilledStar(ImDrawList* dl, float cx, float cy, float r, ImU32 col)
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
//  ClearEditBuffers  -  Reset all edit form fields
// ============================================================
void ClearEditBuffers(UIState& s)
{
    memset(s.editTitle,    0, sizeof(s.editTitle));
    memset(s.editWebsite,  0, sizeof(s.editWebsite));
    memset(s.editUsername, 0, sizeof(s.editUsername));
    memset(s.editPassword, 0, sizeof(s.editPassword));
    memset(s.editNotes,     0, sizeof(s.editNotes));
    memset(s.newTagBuf,     0, sizeof(s.newTagBuf));
    memset(s.editFolderBuf, 0, sizeof(s.editFolderBuf));

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
        THEME::TC(
            ImVec4(0.6588f, 0.6196f, 1.0000f, 1.0f),  // 168,158,255
            ImVec4(0.1176f, 0.3922f, 0.7843f, 1.0f),  // 30,100,200
            theme),
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

    // Dynamic per-tag buttons
    // When a folder is active, only show tags that exist within that folder's entries
    {
        std::vector<std::string> allTags;
        for (const auto& e : s.pm.entries)
        {
            if (!s.filterFolder.empty() && e.folder != s.filterFolder) continue;
            for (const auto& t : e.tags)
                if (std::find(allTags.begin(), allTags.end(), t) == allTags.end())
                    allTags.push_back(t);
        }

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

    // ---- Entry List (folders + entries) ----
    float listH = ImGui::GetContentRegionAvail().y - 56.0f;
    ImGui::BeginChild("##entrylist", ImVec2(W, listH), false);

    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ---- Back row when inside a folder ----
    if (!s.filterFolder.empty())
    {
        ImGui::PushID(9999);
        ImVec2 bPos = ImGui::GetCursorScreenPos();
        const float bH = 38.0f;

		bPos.x += 6.0f;   // left padding for back button and folder name

        ImGui::SetCursorPosX(6.0f);
        if (ImGui::InvisibleButton("##back", ImVec2(W - 12.0f, bH)))
            s.filterFolder = "";

        bool bHov = ImGui::IsItemHovered();

        // Drag-drop target: dropping an entry here removes it from the folder
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload("ENTRY_IDX"))
            {
                int draggedIdx = *(const int*)payload->Data;
                if (draggedIdx >= 0 && draggedIdx < (int)s.pm.entries.size())
                {
                    s.pm.entries[draggedIdx].folder = "";
                    s.pm.SaveToFile();
                }
            }
            bHov = true;   // keep the row highlighted during hover
            ImGui::EndDragDropTarget();
        }

        ImU32 bBg = bHov
            ? THEME::TCU(IM_COL32(28, 40, 72, 230), IM_COL32(196, 210, 238, 230), theme)
            : THEME::TCU(IM_COL32(18, 28, 55, 200), IM_COL32(210, 220, 244, 200), theme);
        dl->AddRectFilled(bPos, ImVec2(bPos.x + W - 12.0f, bPos.y + bH), bBg, 7.0f);

        // Left-arrow
        float ax = bPos.x + 14.0f, ay = bPos.y + bH * 0.5f;
        ImU32 arCol = IM_COL32(90, 160, 255, 220);
        dl->AddTriangleFilled(
            ImVec2(ax + 5, ay - 5), ImVec2(ax + 5, ay + 5), ImVec2(ax - 1, ay), arCol);
        // Horizontal stem
        dl->AddLine(ImVec2(ax - 1, ay), ImVec2(ax + 10, ay), arCol, 1.5f);

        // Folder name
        ImGui::SetCursorScreenPos(
            ImVec2(bPos.x + 30, bPos.y + (bH - ImGui::GetFontSize()) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text,
            THEME::TC(ImVec4(0.68f, 0.84f, 1.0f, 1.0f),
                      ImVec4(0.15f, 0.32f, 0.65f, 1.0f), theme));
        std::string backLabel = s.filterFolder;
        if (backLabel.size() > 20) backLabel = backLabel.substr(0, 18) + "..";
        ImGui::TextUnformatted(backLabel.c_str());
        ImGui::PopStyleColor();

        ImGui::SetCursorScreenPos(ImVec2(bPos.x, bPos.y + bH + 4));
        ImGui::PopID();
    }

    // ---- Folder rows (root view only) ----
    if (s.filterFolder.empty())
    {
        ImVec2 fPos = ImGui::GetCursorScreenPos();
        fPos.x += 6.0f; // left padding for folders

        std::vector<std::string> allFolders = s.pm.GetAllFolders();

        for (size_t fi = 0; fi < allFolders.size(); ++fi)
        {
            const std::string& fn = allFolders[fi];

            // Count how many entries are in this folder
            int cnt = 0;
            for (const auto& e : s.pm.entries)
                if (e.folder == fn) ++cnt;

            ImGui::PushID((int)(2000 + fi));

            const float fH = 54.0f;

            ImGui::SetCursorPosX(6.0f);
            if (ImGui::InvisibleButton("##folder", ImVec2(W - 12.0f, fH)))
            {
                s.filterFolder = fn;
                s.filterTag    = "";
                s.filterFavorites = false;
            }
            bool fHov = ImGui::IsItemHovered();

            // Drag-drop target: an entry dragged onto this row moves into the folder
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload =
                        ImGui::AcceptDragDropPayload("ENTRY_IDX"))
                {
                    int draggedIdx = *(const int*)payload->Data;
                    if (draggedIdx >= 0 && draggedIdx < (int)s.pm.entries.size())
                    {
                        s.pm.entries[draggedIdx].folder = fn;
                        s.pm.SaveToFile();
                    }
                }
                fHov = true;   // keep highlight while hovering a payload
                ImGui::EndDragDropTarget();
            }

            // Right-click context menu
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding,  10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(18, 14));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,   ImVec2(20, 12));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(8, 8));
            if (ImGui::BeginPopupContextItem())
            {
                ImGui::PushStyleColor(ImGuiCol_PopupBg, THEME::TC(
                    ImVec4(0.082f, 0.090f, 0.137f, 1.0f),
                    ImVec4(0.970f, 0.958f, 0.934f, 1.0f), theme));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, THEME::TC(
                    ImVec4(0.192f, 0.176f, 0.20f, 1.0f),
                    ImVec4(0.192f, 0.176f, 0.20f, 1.0f), theme));

                if (cnt == 0)
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.20f, 0.20f, 1.0f));
                    if (ImGui::MenuItem("\t\t(X) Delete Folder"))
                        s.pm.RemoveFolder(fn);
                    ImGui::PopStyleColor();
                }
                else
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
                    ImGui::MenuItem("\t\tFolder is not empty");
                    ImGui::PopStyleColor();
                }

                ImGui::PopStyleColor(2);
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar(4);

            // Draw folder row
            // Background: subtle blue tint, brighter on hover
            ImU32 fBgNorm = THEME::TCU(
                IM_COL32(20, 33, 65, 235), IM_COL32(200, 215, 242, 235), theme);
            ImU32 fBgHov  = THEME::TCU(
                IM_COL32(28, 48, 92, 255), IM_COL32(182, 200, 234, 255), theme);
            dl->AddRectFilled(fPos, ImVec2(fPos.x + W - 12.0f, fPos.y + fH),
                              fHov ? fBgHov : fBgNorm, 8.0f);

            // Folder icon: body + tab
            float ix = fPos.x + 13.0f;
            float iy = fPos.y + fH * 0.5f;
            ImU32 iconFill  = IM_COL32(72, 148, 255, 230);
            ImU32 iconEdge  = IM_COL32(120, 190, 255, 130);
            // Body
            dl->AddRectFilled(ImVec2(ix - 9, iy - 7),  ImVec2(ix + 9, iy + 8),  iconFill, 3.0f);
            // Tab (top-left corner cap)
            dl->AddRectFilled(ImVec2(ix - 9, iy - 11), ImVec2(ix - 2, iy - 5),  iconFill, 2.0f);
            // Subtle edge highlight
            dl->AddRect(ImVec2(ix - 9, iy - 7),  ImVec2(ix + 9, iy + 8),  iconEdge, 3.0f, 0, 1.0f);

            // Folder name
            ImGui::SetCursorScreenPos(ImVec2(fPos.x + 32, fPos.y + 9));
            ImGui::PushStyleColor(ImGuiCol_Text, THEME::TC(
                ImVec4(0.82f, 0.90f, 1.0f, 1.0f),
                ImVec4(0.12f, 0.28f, 0.58f, 1.0f), theme));
            std::string fname = fn;
            if (fname.size() > 22) fname = fname.substr(0, 20) + "..";
            ImGui::TextUnformatted(fname.c_str());
            ImGui::PopStyleColor();

            // Item count
            char cntBuf[24];
            snprintf(cntBuf, sizeof(cntBuf), cnt == 1 ? "1 item" : "%d items", cnt);
            ImGui::SetCursorScreenPos(ImVec2(fPos.x + 32, fPos.y + 30));
            ImGui::PushFont(fontSmall);
            ImGui::PushStyleColor(ImGuiCol_Text, THEME::TC(
                ImVec4(0.48f, 0.64f, 0.88f, 0.80f),
                ImVec4(0.28f, 0.44f, 0.70f, 0.80f), theme));
            ImGui::TextUnformatted(cntBuf);
            ImGui::PopStyleColor();
            ImGui::PopFont();

            // Chevron arrow (right side)
            float arX = fPos.x + W - 20.0f, arY = fPos.y + fH * 0.5f;
            ImU32 arC = IM_COL32(80, 148, 255, fHov ? 220 : 140);
            dl->AddTriangleFilled(
                ImVec2(arX - 4, arY - 5),
                ImVec2(arX - 4, arY + 5),
                ImVec2(arX + 2, arY), arC);

            ImGui::SetCursorScreenPos(ImVec2(fPos.x, fPos.y + fH + 3));
            ImGui::PopID();

			fPos.y += 60.0f;   // extra spacing after each folder row
        }
    }

    // ---- Entry rows ----
    if (filtered.empty() && s.filterFolder.empty()
        && s.pm.GetAllFolders().empty())
    {
        // Truly empty vault
        ImGui::Spacing();
        const char* msg = "No entries yet.";
        float tw = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX((W - tw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.40f, 0.50f, 1.0f));
        ImGui::TextUnformatted(msg);
        ImGui::PopStyleColor();
    }
    else if (filtered.empty() && !s.filterFolder.empty())
    {
        // Inside a folder but it's empty
        ImGui::Spacing();
        const char* msg = "This folder is empty.";
        float tw = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX((W - tw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.40f, 0.50f, 1.0f));
        ImGui::TextUnformatted(msg);
        ImGui::PopStyleColor();
    }
    else if (filtered.empty())
    {
        // Filters / search produced no results
        ImGui::Spacing();
        const char* msg = s.filterFavorites ? "No favorites here." : "No entries found.";
        float tw = ImGui::CalcTextSize(msg).x;
        ImGui::SetCursorPosX((W - tw) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.40f, 0.50f, 1.0f));
        ImGui::TextUnformatted(msg);
        ImGui::PopStyleColor();
    }
    else
    {
        ImVec2 screenPos = ImGui::GetCursorScreenPos();


        for (int idx : filtered)
        {
            const PasswordEntry& e = s.pm.entries[idx];
            bool selected = (idx == s.selectedIdx);

            ImGui::PushID(idx);

            float  itemH = 56.0f;

            // Invisible button for row selection
            if (ImGui::InvisibleButton("##item", ImVec2(W - 12.0f, itemH)))
            {
                s.selectedIdx  = idx;
                s.editMode     = false;
                s.isNewEntry   = false;
                s.showPassword = false;
            }

            // Drag-drop source: drag entry onto a folder row
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload("ENTRY_IDX", &idx, sizeof(int));
                ImGui::PushFont(fontSmall);
                std::string dragLabel = std::string("Move: ") +
                    (e.title.empty() ? "(untitled)" : e.title);
                ImGui::TextUnformatted(dragLabel.c_str());
                ImGui::PopFont();
                ImGui::EndDragDropSource();
            }

            bool hovered = ImGui::IsItemHovered();

            // Right-click: delete login
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(18, 14));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,   ImVec2(20, 12));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,    ImVec2(8, 8));
            if (ImGui::BeginPopupContextItem())
            {
                ImGui::PushStyleColor(ImGuiCol_PopupBg, THEME::TC(
                    ImVec4(0.082f, 0.090f, 0.137f, 1.0f),
                    ImVec4(0.970f, 0.958f, 0.934f, 1.0f), theme));
                ImGui::PushStyleColor(ImGuiCol_Header, THEME::TC(
                    ImVec4(0.25f, 0.25f, 0.40f, 1.0f),
                    ImVec4(0.70f, 0.70f, 0.85f, 1.0f), theme));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, THEME::TC(
                    ImVec4(0.192f, 0.176f, 0.20f, 1.0f),
                    ImVec4(0.192f, 0.176f, 0.20f, 1.0f), theme));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                if (ImGui::MenuItem("\t\t(X) Delete Login"))
                {
                    s.pendingDeleteIdx  = idx;
                    s.showDeleteConfirm = true;
                }
                ImGui::PopStyleColor(4);
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar(4);

            // Row background
            ImU32 bgCol;
            if      (selected) bgCol = THEME::TCU(IM_COL32(168, 158, 255, 255), IM_COL32(30, 100, 200, 255), theme);
            else if (hovered)  bgCol = THEME::TCU(IM_COL32(28, 32, 50, 200),
                                                  IM_COL32(210, 198, 178, 200), theme);
            else               bgCol = IM_COL32(0, 0, 0, 0);
            dl->AddRectFilled(screenPos,
                              ImVec2(screenPos.x + W - 12.0f, screenPos.y + itemH), bgCol, 6.0f);

            // Tag colour dot
            ImVec4 dotColF = e.tags.empty()
                ? ImVec4(0.58f, 0.63f, 0.73f, 1.0f)
                : SETTINGS::GetTagColor(e.tags[0]);
            dl->AddCircleFilled(
                ImVec2(screenPos.x + 16, screenPos.y + itemH * 0.40f), 5.0f,
                ImGui::ColorConvertFloat4ToU32(dotColF), 16);

            // Title
            ImGui::SetCursorScreenPos(ImVec2(screenPos.x + 30, screenPos.y + 9));
            ImGui::PushStyleColor(ImGuiCol_Text, selected
                ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                : THEME::TC(ImVec4(0.886f, 0.902f, 0.941f, 1.0f),
                            ImVec4(0.100f, 0.105f, 0.120f, 1.0f), theme));
            std::string title = e.title.empty() ? "(untitled)" : e.title;
            if (title.size() > 24) title = title.substr(0, 22) + "..";
            ImGui::TextUnformatted(title.c_str());
            ImGui::PopStyleColor();

            // Subtitle: website (small font)
            ImGui::PushFont(fontSmall);
            std::string site = e.website;
            for (const char* pre : { "https://", "http://", "www." })
                if (site.rfind(pre, 0) == 0) { site = site.substr(strlen(pre)); break; }
            if (site.size() > 26) site = site.substr(0, 24) + "..";
            ImGui::SetCursorScreenPos(ImVec2(screenPos.x + 30, screenPos.y + 32));
            ImGui::PushStyleColor(ImGuiCol_Text, selected
                ? ImVec4(0.75f, 0.72f, 1.0f, 1.0f)
                : ImVec4(0.39f, 0.45f, 0.55f, 1.0f));
            ImGui::TextUnformatted(site.c_str());
            ImGui::PopStyleColor();
            ImGui::PopFont();

            // Favorite star
            if (e.isFavorite)
                DrawFilledStar(dl, screenPos.x + W - 28.0f,
                               screenPos.y + itemH * 0.50f, 6.0f, IM_COL32(255, 210, 50, 220));
            else if (hovered || selected)
                DrawFilledStar(dl, screenPos.x + W - 28.0f,
                               screenPos.y + itemH * 0.50f, 6.0f,
                               THEME::TCU(IM_COL32(80, 90, 110, 140),
                                          IM_COL32(160, 155, 145, 140), theme));

            // Star hit-area
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

			screenPos.y += 60.0f;   // next row
        }
    }

    ImGui::EndChild();

    // ---- Footer ----
    ImGui::PushStyleColor(ImGuiCol_Separator, THEME::TC(
        ImVec4(0.118f, 0.137f, 0.212f, 1.0f),
        ImVec4(0.712f, 0.692f, 0.648f, 1.0f), theme));
    ImGui::Separator();
    ImGui::PopStyleColor();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
    ImGui::SetCursorPosX(5);

    if (s.showNewFolderInput)
    {
        // ---- Inline folder-name input ----
        ImGui::SetNextItemWidth(W - 93.0f);
        bool entered = ImGui::InputTextWithHint("##newfolder", "Folder name...",
            s.newFolderBuf, sizeof(s.newFolderBuf),
            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        ImGui::SetItemDefaultFocus();

        ImGui::SameLine(0, 4);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        bool confirmed = ImGui::Button(" OK ");
        ImGui::SameLine(0, 3);
        bool cancelled = ImGui::Button(" X ");
        ImGui::PopStyleColor();

        if (entered || confirmed)
        {
            if (s.newFolderBuf[0] != '\0')
            {
                s.pm.AddFolder(std::string(s.newFolderBuf));
                RENDER::ShowToast("Folder created!", s.toasts);
            }
            memset(s.newFolderBuf, 0, sizeof(s.newFolderBuf));
            s.showNewFolderInput = false;
        }
        else if (cancelled)
        {
            memset(s.newFolderBuf, 0, sizeof(s.newFolderBuf));
            s.showNewFolderInput = false;
        }
    }
    else
    {
        // ---- Normal footer: Add Entry + Create Folder ----
        if (RENDER::GreenButton("  +  Add Entry  "))
        {
            ClearEditBuffers(s);
            s.editMode    = true;
            s.isNewEntry  = true;
            s.selectedIdx = -1;
            // Pre-fill folder when inside one
            if (!s.filterFolder.empty())
                CONVERSIONS::StrToCharBuf(s.filterFolder, s.editFolderBuf,
                                          sizeof(s.editFolderBuf));
        }

        ImGui::SameLine(0, 6);

        // "Create Folder" button - blue-tinted (not theme dependent)  
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.22f, 0.46f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.17f, 0.31f, 0.60f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.10f, 0.18f, 0.38f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        if (ImGui::Button("  Create Folder  "))
            s.showNewFolderInput = true;
        ImGui::PopStyleColor(4);
    }
}

