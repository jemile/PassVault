// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  screens.cpp  -  Full-screen overlay renderers
//  Contains: RenderLockScreen, RenderSettingsScreen
// ============================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "screens.h"
#include "frame.h"
#include "render.h"
#include "settings.h"
#include "updater.h"
#include <cstring>
#include <cmath>
#include <algorithm>


// ============================================================
//  Changelog data  -  shown in the Updates settings tab
// ============================================================
struct ChangelogEntry
{
    const char* version;
    const char* date;
    const char* notes[9];   // nullptr-terminated list of bullet points
};

static const ChangelogEntry CHANGELOG[] =
{
    { "v1.4.0", "May 2026", {
        "User can change master password in security tab.",
        "Folder support for grouping and organizing vault entries",
        "Entries can be moved between folders via drag-and-drop in the sidebar",
        "Sidebar redesigned with collapsible folder tree navigation",
		"Toast notifcations improved with new animations and icons",
        "Settings to configure toast notifications (enable/disable, duration)",
        "Light mode redesigned with sky-blue accent colors throughout",
		"Minimize button and logo added to title bar for better window management",
        nullptr
    }},
    { "v1.3.0", "April 2026", {
        "Deletion for entries and tags using right-click menu item",
		"Confirmation prompts for entry deletion to prevent accidents",
        nullptr
    }},
    { "v1.2.0", "April 2026", {
        "Added Export/Import encrypted vaults",
        "Favorites starring with a dedicated sidebar filter",
        "Custom tags with customizable colors",
        "Added application icon + resource files",
        "Self-updater implemented",
        nullptr
    }},
    { "v1.1.0", "April 2026", {
        "Master password implemented with Argon2id key derivation",
        "Auto-lock on inactivity with configurable timeout",
        "Dark and Light mode toggle using sun and moon",
        "Dedicated settings tab for preferences",
        nullptr
    }},
    { "v1.0.0", "April 2025", {
        "Initial release of PassVault",
        "XSalsa20-Poly1305 encrypted vault",
        "Add entries with customizable data",
        "Built-in password generator with real-time strength meter",
        nullptr
    }},
};


// ============================================================
//  RenderLockScreen  -  Initial setup (first run) or vault unlock
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
        THEME::TCU(IM_COL32(80, 72, 160, 200), IM_COL32(40, 110, 185, 200), theme), 14.0f, 0, 1.5f);

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

	std::string logo = "PassVault " + std::string(UPDATER::CURRENT_VERSION);
	const char* logoCStr = logo.c_str();
    float logoW = ImGui::CalcTextSize(logoCStr).x;
    ImGui::SetCursorPosX((innerW - logoW) * 0.5f + 32);
    ImGui::SetCursorPosY(8);
    ImGui::TextColored(THEME::TC(ImVec4(0.66f, 0.62f, 1.0f, 1.0f), ImVec4(0.08f, 0.46f, 0.88f, 1.0f), theme), logoCStr);
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
        if (SETTINGS::savedToastsEnabled)
        {
            RENDER::ShowToast(s.lockErrMsg, s.toasts, ToastType::Error);
			s.lockErrMsg[0] = '\0'; // clear toast immediately after showing (it will live on in the toast stack)
        }
        else {
            ImGui::PushFont(fontSmall);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.30f, 0.30f, 1.0f));
            float errW = ImGui::CalcTextSize(s.lockErrMsg).x;
            ImGui::SetCursorPosX((innerW - errW) * 0.5f + 32);
            ImGui::TextUnformatted(s.lockErrMsg);
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::Spacing();
        }
    }

    // Action button
    const char* btnLabel = s.lockWorking ? "  Working...  "
                         : isSetup       ? "  Create Vault  "
                                         : "  Unlock  ";
    float btnWidth = ImGui::CalcTextSize(btnLabel).x * 6.0f;

    ImGui::SetCursorPosX(32.0f + (innerW - btnWidth) * 0.5f);
    ImGui::SetCursorPosY(bottomOfCard);

    ImGui::PushStyleColor(ImGuiCol_Button,        THEME::TC(ImVec4(0.235f, 0.220f, 0.470f, 1.0f), ImVec4(0.14f, 0.42f, 0.75f, 1.0f), theme));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, THEME::TC(ImVec4(0.310f, 0.290f, 0.580f, 1.0f), ImVec4(0.18f, 0.50f, 0.84f, 1.0f), theme));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  THEME::TC(ImVec4(0.180f, 0.168f, 0.360f, 1.0f), ImVec4(0.10f, 0.32f, 0.60f, 1.0f), theme));
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

                    if (SETTINGS::savedToastsEnabled)
                    {
                        RENDER::ShowToast("Master password set!", s.toasts, ToastType::Success);
                        s.lockErrMsg[0] = '\0';
                    }
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
                ? s.pm.VerifyMasterPassword(pw)       // vault in memory - verify only
                : s.pm.UnlockWithMasterPassword(pw);  // first unlock - decrypt vault key

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

                if (SETTINGS::savedToastsEnabled)
                {
                    RENDER::ShowToast("Login successful!", s.toasts, ToastType::Success);
                    s.lockErrMsg[0] = '\0'; 
                }
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
//  RenderSettingsScreen  -  User customization and app info
// ============================================================
void RenderSettingsScreen(UIState& s)
{
    using namespace FRAME;

    const float TH          = 40.0f;
    const float cardW       = 600.0f;
    const float contentW    = (float)width;
    const float contentH    = (float)height - TH;
    const float cardH       = std::min(contentH - 60.0f, 730.0f);
    const float cardX       = (contentW - cardW) * 0.5f;
    const float cardY       = (contentH - cardH) * 0.42f;
    const float bottomOfCard = cardH * 0.9f;

    const char* homeBtn = "Go Back";
    float       btnWidth = ImGui::CalcTextSize(homeBtn).x * 6.0f;

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
        THEME::TCU(IM_COL32(80, 72, 160, 200), IM_COL32(40, 110, 185, 200), theme), 14.0f, 0, 1.5f);

    // Widget content via transparent child window
    ImGui::SetCursorPos(ImVec2(cardX, TH + cardY));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,                ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,            THEME::TC(ImVec4(0.08f, 0.07f, 0.17f, 0.60f), ImVec4(0.88f, 0.84f, 0.76f, 0.60f), theme));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,          THEME::TC(ImVec4(0.28f, 0.26f, 0.54f, 1.0f),  ImVec4(0.14f, 0.42f, 0.75f, 1.0f),  theme));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,   THEME::TC(ImVec4(0.38f, 0.35f, 0.70f, 1.0f),  ImVec4(0.18f, 0.50f, 0.84f, 1.0f),  theme));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,    THEME::TC(ImVec4(0.48f, 0.45f, 0.85f, 1.0f),  ImVec4(0.22f, 0.58f, 0.90f, 1.0f),  theme));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32, 24));
    ImGui::BeginChild("##settingscard", ImVec2(cardW, cardH), false, 0);

    const float innerW = cardW - 64.0f;

    // ---- Header ----
    ImGui::PushFont(fontTitle);
    float logoW = ImGui::CalcTextSize("PassVault Settings").x;
    ImGui::SetCursorPosX((innerW - logoW) * 0.5f + 32);
    ImGui::SetCursorPosY(5);
    ImGui::TextColored(THEME::TC(ImVec4(0.66f, 0.62f, 1.0f, 1.0f), ImVec4(0.08f, 0.46f, 0.88f, 1.0f), theme), "PassVault Settings");
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator,
        THEME::TC(ImVec4(0.16f, 0.15f, 0.32f, 1.0f), ImVec4(0.80f, 0.76f, 0.68f, 1.0f), theme));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ---- Tab bar ----
    ImGui::PushStyleColor(ImGuiCol_Tab,
        THEME::TC(ImVec4(0.10f, 0.09f, 0.20f, 1.0f), ImVec4(0.82f, 0.88f, 0.96f, 1.0f), theme));
    ImGui::PushStyleColor(ImGuiCol_TabHovered,
        THEME::TC(ImVec4(0.24f, 0.22f, 0.48f, 1.0f), ImVec4(0.18f, 0.50f, 0.84f, 0.90f), theme));
    ImGui::PushStyleColor(ImGuiCol_TabActive,
        THEME::TC(ImVec4(0.32f, 0.30f, 0.62f, 1.0f), ImVec4(0.14f, 0.42f, 0.75f, 1.0f), theme));
    ImGui::PushStyleColor(ImGuiCol_TabUnfocusedActive,
        THEME::TC(ImVec4(0.20f, 0.18f, 0.40f, 1.0f), ImVec4(0.10f, 0.32f, 0.60f, 1.0f), theme));

    ImGui::SetCursorPosX(5);
    if (ImGui::BeginTabBar("##settingsTabs"))
    {
        // ============================================================
        //  TAB 1 - General
        // ============================================================

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        if (ImGui::BeginTabItem("  General  "))
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                THEME::TCU(IM_COL32(255, 255, 255, 255),
                    IM_COL32(0, 0, 0, 255), theme));

            ImGui::Spacing();

            // Auto-lock
            RENDER::FieldLabel("AUTO-LOCK AFTER INACTIVITY", fontSmall, theme);
            const char* autoLockOptions[] = {
                "30 seconds", "1 minute", "2 minutes", "3 minutes", "Never"
            };
            ImGui::SetNextItemWidth(innerW);
            ImGui::SetCursorPosX(5);
            if (ImGui::Combo("##autolock", &autoLockIndex, autoLockOptions, IM_ARRAYSIZE(autoLockOptions)))
            {
                const float timeouts[] = { 30.0f, 60.0f, 120.0f, 180.0f, -1.0f };
                autoLockTimeout = timeouts[autoLockIndex];
                SETTINGS::Save(theme, autoLockIndex, s.toastsEnabled, s.toastDuration);
            }

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator,
                THEME::TC(ImVec4(0.16f, 0.15f, 0.32f, 1.0f), ImVec4(0.80f, 0.76f, 0.68f, 1.0f), theme));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);

            // Encrypted backup
            RENDER::FieldLabel("ENCRYPTED BACKUP", fontSmall, theme);
            ImGui::SetCursorPosX(5);
            if (RENDER::GreenButton("  Export Backup  "))
                FRAME::ExportBackup();
            ImGui::SameLine();
            if (RENDER::GreenButton("  Import Backup  "))
                FRAME::ImportBackup();

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator,
                THEME::TC(ImVec4(0.16f, 0.15f, 0.32f, 1.0f), ImVec4(0.80f, 0.76f, 0.68f, 1.0f), theme));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);

            // ---- Notifications ----
            RENDER::FieldLabel("NOTIFICATIONS", fontSmall, theme);

            ImGui::SetCursorPosX(5);
            ImGui::PushStyleColor(ImGuiCol_Text, THEME::TC(
                ImVec4(0.86f, 0.88f, 0.94f, 1.0f), ImVec4(0.18f, 0.17f, 0.14f, 1.0f), theme));
            if (ImGui::Checkbox("Enable toast notifications", &s.toastsEnabled))
            {
                RENDER::g_toastsEnabled = s.toastsEnabled;
                SETTINGS::Save(theme, autoLockIndex, s.toastsEnabled, s.toastDuration);
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // Duration slider (only active when toasts are enabled)
            ImGui::BeginDisabled(!s.toastsEnabled);
            RENDER::FieldLabel("NOTIFICATION DURATION", fontSmall, theme);
            ImGui::SetCursorPosX(5);
            ImGui::SetNextItemWidth(innerW - 60.0f);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab,
                THEME::TC(ImVec4(0.50f, 0.46f, 0.90f, 1.0f), ImVec4(0.40f, 0.36f, 0.80f, 1.0f), theme));
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive,
                THEME::TC(ImVec4(0.60f, 0.56f, 1.00f, 1.0f), ImVec4(0.30f, 0.27f, 0.70f, 1.0f), theme));
            ImGui::PushStyleColor(ImGuiCol_FrameBg,
                THEME::TC(ImVec4(0.10f, 0.09f, 0.20f, 1.0f), ImVec4(0.82f, 0.78f, 0.70f, 1.0f), theme));
            if (ImGui::SliderFloat("##toastdur", &s.toastDuration, 2.0f, 8.0f, "%.0f sec"))
            {
                RENDER::g_toastDuration = s.toastDuration;
                SETTINGS::Save(theme, autoLockIndex, s.toastsEnabled, s.toastDuration);
            }
            ImGui::PopStyleColor(3);
            ImGui::SameLine(0, 10);
            ImGui::PushFont(fontSmall);
            ImGui::PushStyleColor(ImGuiCol_Text, THEME::TC(
                ImVec4(0.55f, 0.58f, 0.70f, 1.0f), ImVec4(0.42f, 0.40f, 0.36f, 1.0f), theme));
            ImGui::TextUnformatted("duration");
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::EndDisabled();

            ImGui::SetCursorPosX(128.0f + (innerW - btnWidth) * 0.5f);
            ImGui::SetCursorPosY(bottomOfCard);

            if (RENDER::ThemeButton("  Go Back  ", ImVec2(0, 0)))
                settingsTab = false;

            ImGui::EndTabItem();
            ImGui::PopStyleColor();

        }

        ImGui::PopStyleColor();


        // ============================================================
        //  TAB 2 - Security  (Change Master Password)
        // ============================================================
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        if (ImGui::BeginTabItem("  Security  "))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, 
                THEME::TCU(IM_COL32(255, 255, 255, 255),
                           IM_COL32(0, 0, 0, 255), theme));


            ImGui::Spacing();

            RENDER::FieldLabel("CHANGE MASTER PASSWORD", fontSmall, theme);

            ImGui::PushFont(fontSmall);
            ImGui::PushStyleColor(ImGuiCol_Text, THEME::TC(
                ImVec4(0.55f, 0.58f, 0.70f, 1.0f), ImVec4(0.48f, 0.44f, 0.38f, 1.0f), theme));
            ImGui::SetCursorPosX(5);
            ImGui::TextWrapped("Changing your master password re-encrypts the entire vault.");
            ImGui::PopStyleColor();
            ImGui::PopFont();
            ImGui::Spacing();

            const float cpFieldW = innerW - 62.0f;

            // Current password
            ImGuiInputTextFlags fl0 = s.changePwShowCurrent
                ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_Password;
            ImGui::SetCursorPosX(5);
            ImGui::SetNextItemWidth(cpFieldW);
            ImGui::InputTextWithHint("##cpcur", "Current password",
                s.changePwCurrent, sizeof(s.changePwCurrent), fl0);
            ImGui::SameLine(0, 6);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            if (ImGui::Button(s.changePwShowCurrent ? " Hide##cpcur " : " Show##cpcur "))
                s.changePwShowCurrent = !s.changePwShowCurrent;
            ImGui::PopStyleColor();

            ImGui::Spacing();

            // New password
            ImGuiInputTextFlags fl1 = s.changePwShowNew
                ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_Password;
            ImGui::SetCursorPosX(5);
            ImGui::SetNextItemWidth(cpFieldW);
            ImGui::InputTextWithHint("##cpnew", "New password",
                s.changePwNew, sizeof(s.changePwNew), fl1);
            ImGui::SameLine(0, 6);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            if (ImGui::Button(s.changePwShowNew ? " Hide##cpnew " : " Show##cpnew "))
                s.changePwShowNew = !s.changePwShowNew;
            ImGui::PopStyleColor();

            // Strength bar for new password
            if (s.changePwNew[0] != '\0')
            {
                ImGui::SetCursorPosX(5);
                RENDER::DrawStrengthBar(s.changePwNew, fontSmall, theme);
            }

            ImGui::Spacing();

            // Confirm new password
            ImGuiInputTextFlags fl2 = s.changePwShowConfirm
                ? ImGuiInputTextFlags_None : ImGuiInputTextFlags_Password;
            ImGui::SetCursorPosX(5);
            ImGui::SetNextItemWidth(cpFieldW);
            ImGui::InputTextWithHint("##cpconf", "Confirm new password",
                s.changePwConfirm, sizeof(s.changePwConfirm), fl2);
            ImGui::SameLine(0, 6);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
            if (ImGui::Button(s.changePwShowConfirm ? " Hide##cpconf " : " Show##cpconf "))
                s.changePwShowConfirm = !s.changePwShowConfirm;
            ImGui::PopStyleColor();

            ImGui::Spacing();
            ImGui::Spacing();
            ImGui::SetCursorPosX(5);
            if (RENDER::ThemeButton("  Change Master Password  ", ImVec2(0, 0)))
            {
                std::string cur = s.changePwCurrent;
                std::string newp = s.changePwNew;
                std::string conf = s.changePwConfirm;

                if (cur.empty() || newp.empty() || conf.empty())
                {
                    RENDER::ShowToast("Please fill in all three fields.", s.toasts, ToastType::Warning);
                }
                else if (newp != conf)
                {
                    RENDER::ShowToast("New password does not match", s.toasts, ToastType::Warning);
                }
                else if (newp.size() < 6)
                {
                    RENDER::ShowToast("New password must be at least 6 characters.", s.toasts, ToastType::Warning);
                }
                else if (s.pm.ChangeMasterPassword(cur, newp))
                {
                    memset(s.changePwCurrent, 0, sizeof(s.changePwCurrent));
                    memset(s.changePwNew, 0, sizeof(s.changePwNew));
                    memset(s.changePwConfirm, 0, sizeof(s.changePwConfirm));
                    s.changePwShowCurrent = s.changePwShowNew = s.changePwShowConfirm = false;
                    RENDER::ShowToast("Master password changed successfully!", s.toasts, ToastType::Success);
                }
                else
                {
                    RENDER::ShowToast("Incorrect current password.", s.toasts, ToastType::Error);
                }
            }

            ImGui::SetCursorPosX(128.0f + (innerW - btnWidth) * 0.5f);
            ImGui::SetCursorPosY(bottomOfCard);

            if (RENDER::ThemeButton("  Go Back  ", ImVec2(0, 0)))
                settingsTab = false;

            ImGui::EndTabItem();

            ImGui::PopStyleColor();
        }
        ImGui::PopStyleColor();


        // ============================================================
        //  TAB 3 - Updates  (check + changelog)
        // ============================================================
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        if (ImGui::BeginTabItem("  Updates  "))
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                THEME::TCU(IM_COL32(255, 255, 255, 255), IM_COL32(0, 0, 0, 255), theme));

            ImGui::Spacing();

            // ---- Update check ----
            RENDER::FieldLabel("SOFTWARE UPDATES", fontSmall, theme);

            // Detect state transitions to fire toasts
            static UPDATER::State prevUpState = UPDATER::State::Idle;
            auto upState = UPDATER::state.load(std::memory_order_acquire);

            if (upState != prevUpState)
            {
                if (upState == UPDATER::State::UpToDate)
                {
                    char utMsg[64];
                    snprintf(utMsg, sizeof(utMsg), "You're up to date!  (%s)", UPDATER::CURRENT_VERSION);
                    RENDER::ShowToast(utMsg, s.toasts, ToastType::Success);
                }
                else if (upState == UPDATER::State::Available)
                {
                    char avMsg[128];
                    snprintf(avMsg, sizeof(avMsg), "Update available: %s", UPDATER::latestVersion.c_str());
                    RENDER::ShowToast(avMsg, s.toasts, ToastType::Info);
                }
                else if (upState == UPDATER::State::Error)
                {
                    RENDER::ShowToast("Update check failed.", s.toasts, ToastType::Error);
                }
                else if (upState == UPDATER::State::ReadyToApply)
                {
                    char rdyMsg[64];
                    snprintf(rdyMsg, sizeof(rdyMsg), "%s ready to install.", UPDATER::latestVersion.c_str());
                    RENDER::ShowToast(rdyMsg, s.toasts, ToastType::Info);
                }
                prevUpState = upState;
            }

            // Progress bar or version string
            if (upState == UPDATER::State::Checking)
            {
                ImVec2 barPos = ImGui::GetCursorScreenPos();
                float  barW = innerW, barH = 6.0f;
                float  t    = fmodf((float)ImGui::GetTime() * 0.55f, 1.0f);
                float  segW = barW * 0.35f;
                float  segX = barPos.x + t * (barW + segW) - segW;
                ImDrawList* dl2 = ImGui::GetWindowDrawList();
                dl2->AddRectFilled(barPos, ImVec2(barPos.x + barW, barPos.y + barH),
                    THEME::TCU(IM_COL32(40, 36, 80, 255), IM_COL32(200, 195, 185, 255), theme), 3.0f);
                float x0 = std::max(barPos.x, segX);
                float x1 = std::min(barPos.x + barW, segX + segW);
                if (x1 > x0)
                    dl2->AddRectFilledMultiColor(
                        ImVec2(x0, barPos.y), ImVec2(x1, barPos.y + barH),
                        IM_COL32(108, 100, 220, 0), IM_COL32(108, 100, 220, 255),
                        IM_COL32(108, 100, 220, 255), IM_COL32(108, 100, 220, 0));
                ImGui::Dummy(ImVec2(barW, barH + 4.0f));
                ImGui::PushFont(fontSmall);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.70f, 1.0f));
                ImGui::SetCursorPosX(10);
                ImGui::TextUnformatted("Checking for updates...");
                ImGui::PopStyleColor();
                ImGui::PopFont();
            }
            else if (upState == UPDATER::State::Downloading)
            {
                float prog = UPDATER::downloadProgress.load(std::memory_order_relaxed);
                ImVec2 barPos = ImGui::GetCursorScreenPos();
                float  barW = innerW, barH = 6.0f;
                ImDrawList* dl2 = ImGui::GetWindowDrawList();
                dl2->AddRectFilled(barPos, ImVec2(barPos.x + barW, barPos.y + barH),
                    THEME::TCU(IM_COL32(40, 36, 80, 255), IM_COL32(200, 195, 185, 255), theme), 3.0f);
                if (prog > 0.0f)
                    dl2->AddRectFilled(barPos, ImVec2(barPos.x + barW * prog, barPos.y + barH),
                        IM_COL32(108, 100, 220, 255), 3.0f);
                ImGui::Dummy(ImVec2(barW, barH + 4.0f));
                ImGui::PushFont(fontSmall);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.58f, 0.70f, 1.0f));
                char dlMsg[48];
                snprintf(dlMsg, sizeof(dlMsg), "Downloading...  %.0f%%", prog * 100.0f);
                ImGui::SetCursorPosX(10);
                ImGui::TextUnformatted(dlMsg);
                ImGui::PopStyleColor();
                ImGui::PopFont();
            }
            else
            {
                ImGui::PushFont(fontSmall);
                ImGui::PushStyleColor(ImGuiCol_Text, THEME::TC(
                    ImVec4(0.45f, 0.48f, 0.60f, 1.0f), ImVec4(0.42f, 0.40f, 0.36f, 1.0f), theme));
                static const std::string verStr =
                    "Current version: " + std::string(UPDATER::CURRENT_VERSION);
                ImGui::SetCursorPosX(10);
                ImGui::TextUnformatted(verStr.c_str());
                ImGui::PopStyleColor();
                ImGui::PopFont();
            }

            // Action buttons
            ImGui::SetCursorPosX(5);
            if (upState == UPDATER::State::Available)
            {
                if (RENDER::GreenButton("  Download & Install  "))
                    UPDATER::StartDownload();
            }
            else if (upState == UPDATER::State::ReadyToApply)
            {
                if (RENDER::GreenButton("  Restart & Apply  "))
                {
                    UPDATER::ApplyUpdate();
                    FRAME::shouldExit = true;
                }
            }
            else if (upState != UPDATER::State::Downloading)
            {
                bool disableBtn = (upState == UPDATER::State::UpToDate ||
                    upState == UPDATER::State::Checking);
                ImGui::BeginDisabled(disableBtn);
                if (RENDER::ThemeButton("  Check for Updates  ", ImVec2(0, 0)))
                    UPDATER::StartCheck();
                ImGui::EndDisabled();
            }

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Separator,
                THEME::TC(ImVec4(0.16f, 0.15f, 0.32f, 1.0f), ImVec4(0.80f, 0.76f, 0.68f, 1.0f), theme));
            ImGui::Separator();
            ImGui::PopStyleColor();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 6);

            // ---- Changelog ----
            RENDER::FieldLabel("WHAT'S NEW", fontSmall, theme);
            ImGui::Spacing();

            // Scrollable changelog area - height fills remaining card space minus Go Back row
            const float clH = bottomOfCard - ImGui::GetCursorPosY() - 52.0f;
            ImGui::PushStyleColor(ImGuiCol_ChildBg,           ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,       THEME::TC(ImVec4(0.08f, 0.07f, 0.17f, 0.0f),  ImVec4(0.88f, 0.84f, 0.76f, 0.0f),  theme));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,     THEME::TC(ImVec4(0.28f, 0.26f, 0.54f, 1.0f),  ImVec4(0.14f, 0.42f, 0.75f, 1.0f),  theme));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, THEME::TC(ImVec4(0.38f, 0.35f, 0.70f, 1.0f), ImVec4(0.18f, 0.50f, 0.84f, 1.0f), theme));
            ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,  THEME::TC(ImVec4(0.48f, 0.45f, 0.85f, 1.0f), ImVec4(0.22f, 0.58f, 0.90f, 1.0f), theme));
            ImGui::BeginChild("##changelog", ImVec2(innerW, clH), false, 0);

            for (const auto& entry : CHANGELOG)
            {
                // Version badge + date on the same line
                ImGui::PushFont(font);
                ImGui::PushStyleColor(ImGuiCol_Text,
                    THEME::TC(ImVec4(0.66f, 0.62f, 1.0f, 1.0f),
                              ImVec4(0.08f, 0.46f, 0.88f, 1.0f), theme));
                ImGui::SetCursorPosX(5);
                ImGui::TextUnformatted(entry.version);
                ImGui::PopStyleColor();
                ImGui::PopFont();

                ImGui::SameLine(0, 10);

                ImGui::PushFont(fontSmall);
                ImGui::PushStyleColor(ImGuiCol_Text, THEME::TC(
                    ImVec4(0.45f, 0.48f, 0.60f, 1.0f),
                    ImVec4(0.48f, 0.44f, 0.38f, 1.0f), theme));
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);  // align baseline with larger font
                ImGui::TextUnformatted(entry.date);
                ImGui::PopStyleColor();
                ImGui::PopFont();

                // Bullet points
                ImGui::PushFont(fontSmall);
                ImGui::PushStyleColor(ImGuiCol_Text, THEME::TC(
                    ImVec4(0.76f, 0.78f, 0.86f, 1.0f),
                    ImVec4(0.22f, 0.20f, 0.18f, 1.0f), theme));
                for (int i = 0; entry.notes[i] != nullptr; ++i)
                {
                    ImGui::SetCursorPosX(14);
                    ImGui::BulletText(entry.notes[i]);  // Bullet point   
                }
                ImGui::PopStyleColor();
                ImGui::PopFont();

                ImGui::Spacing();
                ImGui::SetCursorPosX(5);
                ImGui::PushStyleColor(ImGuiCol_Separator,
                    THEME::TC(ImVec4(0.14f, 0.13f, 0.28f, 1.0f),
                              ImVec4(0.82f, 0.78f, 0.70f, 1.0f), theme));
                ImGui::Separator();
                ImGui::PopStyleColor();
                ImGui::Spacing();
            }

            ImGui::EndChild();
            ImGui::PopStyleColor(5);  // ChildBg + 4 scrollbar colors

            ImGui::SetCursorPosX(128.0f + (innerW - btnWidth) * 0.5f);
            ImGui::SetCursorPosY(bottomOfCard);

            if (RENDER::ThemeButton("  Go Back  ", ImVec2(0, 0)))
                settingsTab = false;

            ImGui::EndTabItem();
            ImGui::PopStyleColor();
        }
        ImGui::PopStyleColor();


        ImGui::EndTabBar(); // Finalizes the entire tab bar
        ImGui::PopStyleColor(4);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();                 // WindowPadding
    ImGui::PopStyleColor(5);              // ChildBg + 4 scrollbar colors
}