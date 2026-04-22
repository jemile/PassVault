// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  popups.cpp  –  Modal popup renderers
// ============================================================

#include "popups.h"
#include "frame.h"
#include "render.h"


// ============================================================
//  RenderGenPopup  –  Password generator modal
// ============================================================
void RenderGenPopup(UIState& s)
{
    using namespace FRAME;

    if (!s.showGenPopup) return;

    ImGui::OpenPopup("Password Generator");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 300));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, THEME::TC(
        ImVec4(0.082f, 0.090f, 0.137f, 1.0f),
        ImVec4(0.970f, 0.958f, 0.934f, 1.0f), theme));

    if (ImGui::BeginPopupModal("Password Generator", &s.showGenPopup,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::PushFont(fontTitle);
        ImGui::TextColored(ImVec4(0.66f, 0.62f, 1.0f, 1.0f), "Password Generator");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();

        // Length slider
        RENDER::FieldLabel("LENGTH", fontSmall, theme);
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##genlen", &s.genLength, 8, 64);
        ImGui::Spacing();

        // Character set toggles
        RENDER::FieldLabel("CHARACTER SETS", fontSmall, theme);
        ImGui::Checkbox("Uppercase (A-Z)",   &s.genUpper);
        ImGui::SameLine(210);
        ImGui::Checkbox("Lowercase (a-z)",   &s.genLower);
        ImGui::Checkbox("Numbers  (0-9)",    &s.genDigits);
        ImGui::SameLine(210);
        ImGui::Checkbox("Symbols  (!@#...)", &s.genSymbols);
        ImGui::Spacing();

        // Preview field
        RENDER::FieldLabel("PREVIEW", fontSmall, theme);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, THEME::TC(
            ImVec4(0.055f, 0.063f, 0.098f, 1.0f),
            ImVec4(0.868f, 0.852f, 0.820f, 1.0f), theme));
        ImGui::SetNextItemWidth(-80);
        ImGui::InputText("##genprev", s.genPreview, sizeof(s.genPreview),
            ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor();

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        if (ImGui::Button("Roll", ImVec2(70, 0)))
        {
            std::string pw = PasswordManager::GeneratePassword(
                s.genLength, s.genUpper, s.genLower, s.genDigits, s.genSymbols);
            CONVERSIONS::StrToCharBuf(pw, s.genPreview, sizeof(s.genPreview));
        }
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (RENDER::GreenButton("  Use This Password  "))
        {
            CONVERSIONS::StrToCharBuf(s.genPreview, s.editPassword, sizeof(s.editPassword));
            s.showGenPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        if (ImGui::Button("  Cancel  "))
        {
            s.showGenPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}


// ============================================================
//  RenderDeleteConfirmPopup  –  Entry deletion confirmation modal
// ============================================================
void RenderDeleteConfirmPopup(UIState& s)
{
    using namespace FRAME;

    if (!s.showDeleteConfirm) return;

    ImGui::OpenPopup("Confirm Delete");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(340, 208));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, THEME::TC(
        ImVec4(0.082f, 0.090f, 0.137f, 1.0f),
        ImVec4(0.970f, 0.958f, 0.934f, 1.0f), theme));

    if (ImGui::BeginPopupModal("Confirm Delete", &s.showDeleteConfirm,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Spacing();
        ImGui::PushFont(fontTitle);
        ImGui::TextColored(ImVec4(0.96f, 0.30f, 0.30f, 1.0f), "Delete Entry?");
        ImGui::PopFont();
        ImGui::Spacing();

        if (s.selectedIdx >= 0 && s.selectedIdx < (int)s.pm.entries.size())
        {
            ImGui::TextWrapped("Are you sure you want to delete \"%s\"? This cannot be undone.",
                s.pm.entries[s.selectedIdx].title.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (RENDER::RedButton("  Yes, Delete  "))
        {
            if (s.selectedIdx >= 0 && s.selectedIdx < (int)s.pm.entries.size())
                s.pm.RemoveEntry(s.pm.entries[s.selectedIdx].id);

            s.selectedIdx       = -1;
            s.editMode          = false;
            s.isNewEntry        = false;
            s.showDeleteConfirm = false;
            RENDER::ShowToast("Entry deleted.", s.toastMsg, s.toastTimer);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("  Cancel  "))
        {
            s.showDeleteConfirm = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}
