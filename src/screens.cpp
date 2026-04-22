// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  screens.cpp  –  Full-screen overlay renderers
//  Contains: RenderLockScreen, RenderSettingsScreen
// ============================================================

#include "screens.h"
#include "frame.h"
#include "render.h"
#include <GLFW/glfw3.h>
#include <cstring>


// ============================================================
//  RenderLockScreen  –  Initial setup (first run) or vault unlock
// ============================================================
void RenderLockScreen(UIState& s)
{
    using namespace FRAME;

    const bool  isSetup  = (s.appState == AppState::Setup);
    const float TH       = 40.0f;
    const float cardW    = 420.0f;
    const float cardH    = isSetup ? 358.0f : 298.0f;
    const float contentW = (float)width;
    const float contentH = (float)height - TH;
    const float cardX    = (contentW - cardW) * 0.5f;
    const float cardY    = (contentH - cardH) * 0.42f;  // slightly above center

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    ImVec2      winSP = ImGui::GetWindowPos();

    // Card shadow
    ImVec2 csp(winSP.x + cardX, winSP.y + TH + cardY);
    dl->AddRectFilled(
        ImVec2(csp.x + 5, csp.y + 6),
        ImVec2(csp.x + cardW + 5, csp.y + cardH + 6),
        THEME::TCU(IM_COL32(0, 0, 0, 70), IM_COL32(0, 0, 0, 28), theme), 14.0f);

    // Card body
    dl->AddRectFilled(csp, ImVec2(csp.x + cardW, csp.y + cardH),
        THEME::TCU(IM_COL32(22, 20, 46, 255), IM_COL32(250, 246, 238, 255), theme), 14.0f);
    dl->AddRect(csp, ImVec2(csp.x + cardW, csp.y + cardH),
        THEME::TCU(IM_COL32(80, 72, 160, 200), IM_COL32(185, 172, 148, 200), theme), 14.0f, 0, 1.5f);

    // Widget content via transparent child window
    ImGui::SetCursorPos(ImVec2(cardX, TH + cardY));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32, 24));
    ImGui::BeginChild("##lockcard", ImVec2(cardW, cardH), false,
        ImGuiWindowFlags_NoScrollbar);

    const float innerW       = cardW - 64.0f;
    const float bottomOfCard = cardH * 0.75f;

    // Logo
    ImGui::PushFont(fontTitle);
    float logoW = ImGui::CalcTextSize("PassVault").x;
    ImGui::SetCursorPosX((innerW - logoW) * 0.5f + 32);
    ImGui::SetCursorPosY(5);
    ImGui::TextColored(ImVec4(0.66f, 0.62f, 1.0f, 1.0f), "PassVault");
    ImGui::PopFont();

    // Subtitle
    ImGui::Spacing();
    const char* sub = isSetup
        ? "Create your master password to get started."
        : "Enter your master password to unlock your vault.";
    ImGui::PushFont(fontSmall);
    float subW = ImGui::CalcTextSize(sub).x;
    ImGui::SetCursorPosX((innerW - std::min(subW, innerW)) * 0.5f + 32);
    ImGui::PushStyleColor(ImGuiCol_Text, THEME::TC(
        ImVec4(0.55f, 0.58f, 0.70f, 1.0f),
        ImVec4(0.42f, 0.40f, 0.36f, 1.0f), theme));
    ImGui::TextWrapped("%s", sub);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator,
        THEME::TC(ImVec4(0.16f, 0.15f, 0.32f, 1.0f), ImVec4(0.80f, 0.76f, 0.68f, 1.0f), theme));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Spacing();

    // Master password field
    RENDER::FieldLabel("MASTER PASSWORD", fontSmall, theme);
    float pwFieldW = innerW - 8.0f;
    ImGuiInputTextFlags pwFlags = s.lockShowPw ? ImGuiInputTextFlags_None
                                               : ImGuiInputTextFlags_Password;
    ImGui::SetCursorPosX(5);
    ImGui::SetNextItemWidth(pwFieldW);
    ImGui::InputText("##lockpw", s.lockPwBuf, sizeof(s.lockPwBuf), pwFlags);
    ImGui::SameLine(0, 6);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if (ImGui::Button(s.lockShowPw ? " Hide##cc " : " Show##cc "))
        s.lockShowPw = !s.lockShowPw;
    ImGui::PopStyleColor();

    // Confirm password field (setup only)
    if (isSetup)
    {
        ImGui::Spacing();
        RENDER::FieldLabel("CONFIRM PASSWORD", fontSmall, theme);
        ImGuiInputTextFlags cfFlags = s.lockShowConfirm ? ImGuiInputTextFlags_None
                                                        : ImGuiInputTextFlags_Password;
        ImGui::SetCursorPosX(5);
        ImGui::SetNextItemWidth(pwFieldW);
        ImGui::InputText("##lockconfirm", s.lockConfirmBuf, sizeof(s.lockConfirmBuf), cfFlags);
        ImGui::SameLine(0, 6);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        if (ImGui::Button(s.lockShowConfirm ? " Hide##xx " : " Show##xxx "))
            s.lockShowConfirm = !s.lockShowConfirm;
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Error message
    if (s.lockErrMsg[0] != '\0')
    {
        ImGui::PushFont(fontSmall);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.30f, 0.30f, 1.0f));
        float errW = ImGui::CalcTextSize(s.lockErrMsg).x;
        ImGui::SetCursorPosX((innerW - errW) * 0.5f + 32);
        ImGui::TextUnformatted(s.lockErrMsg);
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::Spacing();
    }

    // Action button
    const char* btnLabel = s.lockWorking ? "  Working...  "
                         : isSetup       ? "  Create Vault  "
                                         : "  Unlock  ";
    float btnWidth = ImGui::CalcTextSize(btnLabel).x * 6.0f;

    ImGui::SetCursorPosX(32.0f + (innerW - btnWidth) * 0.5f);
    ImGui::SetCursorPosY(bottomOfCard);

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.235f, 0.220f, 0.470f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.310f, 0.290f, 0.580f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.180f, 0.168f, 0.360f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f,   1.0f,   1.0f,   1.0f));
    bool pressed = ImGui::Button(btnLabel, ImVec2(btnWidth, 0)) && !s.lockWorking;
    bool enterPress = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter);
    ImGui::PopStyleColor(4);

    // Handle button press or Enter key
    if (pressed || enterPress)
    {
        s.lockErrMsg[0] = '\0';
        std::string pw(s.lockPwBuf);

        if (pw.empty())
        {
            CONVERSIONS::StrToCharBuf("Please enter a password.", s.lockErrMsg, sizeof(s.lockErrMsg));
        }
        else if (isSetup)
        {
            if (pw != std::string(s.lockConfirmBuf))
            {
                CONVERSIONS::StrToCharBuf("Passwords do not match.", s.lockErrMsg, sizeof(s.lockErrMsg));
            }
            else if (pw.size() < 6)
            {
                CONVERSIONS::StrToCharBuf("Password must be at least 6 characters.",
                    s.lockErrMsg, sizeof(s.lockErrMsg));
            }
            else
            {
                s.lockWorking = true;
                if (s.pm.SetupMasterPassword(pw))
                {
                    memset(s.lockPwBuf,      0, sizeof(s.lockPwBuf));
                    memset(s.lockConfirmBuf, 0, sizeof(s.lockConfirmBuf));
                    s.lockShowPw      = false;
                    s.lockShowConfirm = false;
                    s.appState        = AppState::Vault;
                    s.lastActivityTime = glfwGetTime();
                }
                else
                {
                    CONVERSIONS::StrToCharBuf("Failed to create vault. Try again.",
                        s.lockErrMsg, sizeof(s.lockErrMsg));
                }
                s.lockWorking = false;
            }
        }
        else  // Unlock
        {
            s.lockWorking = true;
            bool ok = s.pmInitialized
                ? s.pm.VerifyMasterPassword(pw)       // vault in memory – verify only
                : s.pm.UnlockWithMasterPassword(pw);  // first unlock – decrypt vault key

            if (ok)
            {
                if (!s.pmInitialized)
                {
                    s.pm.LoadFromFile();
                    s.pmInitialized = true;
                    std::string genPw = PasswordManager::GeneratePassword(
                        s.genLength, s.genUpper, s.genLower, s.genDigits, s.genSymbols);
                    CONVERSIONS::StrToCharBuf(genPw, s.genPreview, sizeof(s.genPreview));
                }
                memset(s.lockPwBuf, 0, sizeof(s.lockPwBuf));
                s.lockShowPw       = false;
                s.appState         = AppState::Vault;
                s.lastActivityTime = glfwGetTime();
            }
            else
            {
                CONVERSIONS::StrToCharBuf("Incorrect password. Please try again.",
                    s.lockErrMsg, sizeof(s.lockErrMsg));
            }
            s.lockWorking = false;
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}


// ============================================================
//  RenderSettingsScreen  –  User customization and app info
// ============================================================
void RenderSettingsScreen(UIState& /*s*/)
{
    using namespace FRAME;

    const float TH          = 40.0f;
    const float cardW       = 600.0f;
    const float cardH       = 500.0f;
    const float contentW    = (float)width;
    const float contentH    = (float)height - TH;
    const float cardX       = (contentW - cardW) * 0.5f;
    const float cardY       = (contentH - cardH) * 0.42f;  // slightly above center
    const float bottomOfCard = cardH * 0.9f;

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    ImVec2      winSP = ImGui::GetWindowPos();

    // Card shadow
    ImVec2 csp(winSP.x + cardX, winSP.y + TH + cardY);
    dl->AddRectFilled(
        ImVec2(csp.x + 5, csp.y + 6),
        ImVec2(csp.x + cardW + 5, csp.y + cardH + 6),
        THEME::TCU(IM_COL32(0, 0, 0, 70), IM_COL32(0, 0, 0, 28), theme), 14.0f);

    // Card body
    dl->AddRectFilled(csp, ImVec2(csp.x + cardW, csp.y + cardH),
        THEME::TCU(IM_COL32(22, 20, 46, 255), IM_COL32(250, 246, 238, 255), theme), 14.0f);
    dl->AddRect(csp, ImVec2(csp.x + cardW, csp.y + cardH),
        THEME::TCU(IM_COL32(80, 72, 160, 200), IM_COL32(185, 172, 148, 200), theme), 14.0f, 0, 1.5f);

    // Widget content via transparent child window
    ImGui::SetCursorPos(ImVec2(cardX, TH + cardY));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32, 24));
    ImGui::BeginChild("##settingscard", ImVec2(cardW, cardH), false,
        ImGuiWindowFlags_NoScrollbar);

    const float innerW = cardW - 64.0f;

    // Logo
    ImGui::PushFont(fontTitle);
    float logoW = ImGui::CalcTextSize("PassVault Settings").x;
    ImGui::SetCursorPosX((innerW - logoW) * 0.5f + 32);
    ImGui::SetCursorPosY(5);
    ImGui::TextColored(ImVec4(0.66f, 0.62f, 1.0f, 1.0f), "PassVault Settings");
    ImGui::PopFont();

    // Subtitle
    ImGui::Spacing();
    const char* sub = "User Customization and App Info";
    ImGui::PushFont(fontSmall);
    float subW = ImGui::CalcTextSize(sub).x;
    ImGui::SetCursorPosX((innerW - std::min(subW, innerW)) * 0.5f + 32);
    ImGui::PushStyleColor(ImGuiCol_Text, THEME::TC(
        ImVec4(0.55f, 0.58f, 0.70f, 1.0f),
        ImVec4(0.42f, 0.40f, 0.36f, 1.0f), theme));
    ImGui::TextWrapped("%s", sub);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator,
        THEME::TC(ImVec4(0.16f, 0.15f, 0.32f, 1.0f), ImVec4(0.80f, 0.76f, 0.68f, 1.0f), theme));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Spacing();

    // Auto-lock setting
    RENDER::FieldLabel("AUTO-LOCK AFTER INACTIVITY", fontSmall, theme);
    const char* autoLockOptions[] = {
        "30 seconds",
        "1 minute",
        "2 minutes",
        "3 minutes",
        "Never"
    };
    ImGui::SetNextItemWidth(innerW);
    ImGui::SetCursorPosX(5);
    if (ImGui::Combo("##autolock", &autoLockIndex, autoLockOptions, IM_ARRAYSIZE(autoLockOptions)))
    {
        const float timeouts[] = { 30.0f, 60.0f, 120.0f, 180.0f, -1.0f };
        autoLockTimeout = timeouts[autoLockIndex];
    }

    // Go Back button
    const char* homeBtn  = "Go Back";
    float       btnWidth = ImGui::CalcTextSize(homeBtn).x * 6.0f;

    ImGui::SetCursorPosX(32.0f + (innerW - btnWidth) * 0.5f);
    ImGui::SetCursorPosY(bottomOfCard);

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.235f, 0.220f, 0.470f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.310f, 0.290f, 0.580f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.180f, 0.168f, 0.360f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f,   1.0f,   1.0f,   1.0f));
    if (ImGui::Button(homeBtn, ImVec2(btnWidth, 0)))
        settingsTab = false;
    ImGui::PopStyleColor(4);

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}
