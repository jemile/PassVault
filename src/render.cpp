// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  render.cpp  -  Theme tables and reusable ImGui widget helpers
// ============================================================

#include "render.h"
#include <string>
#include <cstring>
#include <vector>
#include <algorithm>


// ============================================================
//  THEME::DarkTheme
// ============================================================
void THEME::DarkTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4*     c = s.Colors;

    // Background
    c[ImGuiCol_WindowBg] = ImVec4(0.067f, 0.075f, 0.118f, 1.0f);
    c[ImGuiCol_ChildBg]  = ImVec4(0.082f, 0.090f, 0.137f, 1.0f);
    c[ImGuiCol_PopupBg]  = ImVec4(0.090f, 0.100f, 0.153f, 1.0f);

    // Borders
    c[ImGuiCol_Border]       = ImVec4(0.118f, 0.137f, 0.212f, 1.0f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.0f,   0.0f,   0.0f,   0.0f);

    // Text
    c[ImGuiCol_Text]         = ImVec4(0.886f, 0.902f, 0.941f, 1.0f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.392f, 0.455f, 0.545f, 1.0f);

    // Frames (input fields, combo boxes, etc.)
    c[ImGuiCol_FrameBg]        = ImVec4(0.110f, 0.125f, 0.196f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.145f, 0.162f, 0.245f, 1.0f);
    c[ImGuiCol_FrameBgActive]  = ImVec4(0.175f, 0.195f, 0.290f, 1.0f);

    // Title bar
    c[ImGuiCol_TitleBg]          = ImVec4(0.055f, 0.063f, 0.098f, 1.0f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(0.067f, 0.075f, 0.118f, 1.0f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.055f, 0.063f, 0.098f, 1.0f);

    // Menu bar
    c[ImGuiCol_MenuBarBg] = ImVec4(0.047f, 0.055f, 0.086f, 1.0f);

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.055f, 0.063f, 0.098f, 1.0f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.235f, 0.220f, 0.470f, 1.0f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.310f, 0.290f, 0.580f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.424f, 0.388f, 0.760f, 1.0f);

    // Buttons
    c[ImGuiCol_Button]        = ImVec4(0.235f, 0.220f, 0.470f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.310f, 0.290f, 0.580f, 1.0f);
    c[ImGuiCol_ButtonActive]  = ImVec4(0.180f, 0.168f, 0.360f, 1.0f);

    // Headers / selectables
    c[ImGuiCol_Header]        = ImVec4(0.235f, 0.220f, 0.470f, 0.7f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.235f, 0.220f, 0.470f, 0.9f);
    c[ImGuiCol_HeaderActive]  = ImVec4(0.235f, 0.220f, 0.470f, 1.0f);

    // Separator
    c[ImGuiCol_Separator]        = ImVec4(0.118f, 0.137f, 0.212f, 1.0f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.424f, 0.388f, 0.863f, 0.8f);
    c[ImGuiCol_SeparatorActive]  = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);

    // Resize grip
    c[ImGuiCol_ResizeGrip]        = ImVec4(0.235f, 0.220f, 0.470f, 0.5f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.424f, 0.388f, 0.863f, 0.8f);
    c[ImGuiCol_ResizeGripActive]  = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);

    // Tabs
    c[ImGuiCol_Tab]                = ImVec4(0.082f, 0.090f, 0.137f, 1.0f);
    c[ImGuiCol_TabHovered]         = ImVec4(0.310f, 0.290f, 0.580f, 0.9f);
    c[ImGuiCol_TabActive]          = ImVec4(0.235f, 0.220f, 0.470f, 1.0f);
    c[ImGuiCol_TabUnfocused]       = ImVec4(0.067f, 0.075f, 0.118f, 1.0f);
    c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.145f, 0.137f, 0.275f, 1.0f);

    // Misc
    c[ImGuiCol_CheckMark]            = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.520f, 0.490f, 0.950f, 1.0f);
    c[ImGuiCol_TextSelectedBg]       = ImVec4(0.235f, 0.220f, 0.470f, 0.6f);
    c[ImGuiCol_DragDropTarget]       = ImVec4(0.424f, 0.388f, 0.863f, 0.9f);
    c[ImGuiCol_NavHighlight]         = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.0f,   0.0f,   0.0f,   0.6f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.520f, 0.490f, 0.950f, 1.0f);
}


// ============================================================
//  THEME::LightTheme
// ============================================================
void THEME::LightTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4*     c = s.Colors;

    // Background
    c[ImGuiCol_WindowBg] = ImVec4(0.960f, 0.948f, 0.922f, 1.0f);
    c[ImGuiCol_ChildBg]  = ImVec4(0.970f, 0.958f, 0.934f, 1.0f);
    c[ImGuiCol_PopupBg]  = ImVec4(0.970f, 0.958f, 0.934f, 1.0f);

    // Borders
    c[ImGuiCol_Border]       = ImVec4(0.712f, 0.692f, 0.648f, 1.0f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.0f,   0.0f,   0.0f,   0.0f);

    // Text
    c[ImGuiCol_Text]         = ImVec4(0.100f, 0.105f, 0.120f, 1.0f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.500f, 0.520f, 0.550f, 1.0f);

    // Frames
    c[ImGuiCol_FrameBg]        = ImVec4(0.868f, 0.852f, 0.820f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.944f, 0.930f, 0.904f, 1.0f);
    c[ImGuiCol_FrameBgActive]  = ImVec4(0.950f, 0.938f, 0.912f, 1.0f);

    // Title bar
    c[ImGuiCol_TitleBg]          = ImVec4(0.944f, 0.930f, 0.904f, 1.0f);
    c[ImGuiCol_TitleBgActive]    = ImVec4(0.960f, 0.948f, 0.922f, 1.0f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.944f, 0.930f, 0.904f, 1.0f);

    // Menu bar
    c[ImGuiCol_MenuBarBg] = ImVec4(0.950f, 0.938f, 0.912f, 1.0f);

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.944f, 0.930f, 0.904f, 1.0f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.14f,  0.42f,  0.75f,  1.0f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f,  0.50f,  0.84f,  1.0f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.22f,  0.58f,  0.90f,  1.0f);

    // Buttons
    c[ImGuiCol_Button]        = ImVec4(0.14f,  0.42f,  0.75f,  1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.18f,  0.50f,  0.84f,  1.0f);
    c[ImGuiCol_ButtonActive]  = ImVec4(0.10f,  0.32f,  0.60f,  1.0f);

    // Headers / selectables
    c[ImGuiCol_Header]        = ImVec4(0.14f,  0.42f,  0.75f,  0.7f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.14f,  0.42f,  0.75f,  0.9f);
    c[ImGuiCol_HeaderActive]  = ImVec4(0.14f,  0.42f,  0.75f,  1.0f);

    // Separator
    c[ImGuiCol_Separator]        = ImVec4(0.712f, 0.692f, 0.648f, 1.0f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.14f,  0.48f,  0.88f,  0.8f);
    c[ImGuiCol_SeparatorActive]  = ImVec4(0.14f,  0.48f,  0.88f,  1.0f);

    // Resize grip
    c[ImGuiCol_ResizeGrip]        = ImVec4(0.14f,  0.42f,  0.75f,  0.5f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.14f,  0.48f,  0.88f,  0.8f);
    c[ImGuiCol_ResizeGripActive]  = ImVec4(0.14f,  0.48f,  0.88f,  1.0f);

    // Tabs
    c[ImGuiCol_Tab]                = ImVec4(0.960f, 0.948f, 0.922f, 1.0f);
    c[ImGuiCol_TabHovered]         = ImVec4(0.18f,  0.50f,  0.84f,  0.9f);
    c[ImGuiCol_TabActive]          = ImVec4(0.14f,  0.42f,  0.75f,  1.0f);
    c[ImGuiCol_TabUnfocused]       = ImVec4(0.944f, 0.930f, 0.904f, 1.0f);
    c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.950f, 0.938f, 0.912f, 1.0f);

    // Misc
    c[ImGuiCol_CheckMark]            = ImVec4(0.10f,  0.46f,  0.88f,  1.0f);
    c[ImGuiCol_SliderGrab]           = ImVec4(0.10f,  0.46f,  0.88f,  1.0f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.16f,  0.56f,  0.94f,  1.0f);
    c[ImGuiCol_TextSelectedBg]       = ImVec4(0.14f,  0.42f,  0.75f,  0.6f);
    c[ImGuiCol_DragDropTarget]       = ImVec4(0.10f,  0.46f,  0.88f,  0.9f);
    c[ImGuiCol_NavHighlight]         = ImVec4(0.10f,  0.46f,  0.88f,  1.0f);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.0f,   0.0f,   0.0f,   0.4f);
    c[ImGuiCol_PlotHistogram]        = ImVec4(0.10f,  0.46f,  0.88f,  1.0f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.16f,  0.56f,  0.94f,  1.0f);
}


// ============================================================
//  CONVERSIONS::StrToCharBuf
// ============================================================
void CONVERSIONS::StrToCharBuf(const std::string& src, char* dst, size_t dstSize)
{
    strncpy_s(dst, dstSize, src.c_str(), _TRUNCATE);
}


// ============================================================
//  RENDER module globals  (synced from UIState / FRAME each frame)
// ============================================================
bool  RENDER::g_toastsEnabled = true;
float RENDER::g_toastDuration = 4.0f;
int   RENDER::g_theme         = 0;   // 0 = dark, 1 = light


// ============================================================
//  RENDER helpers
// ============================================================

void RENDER::ShowToast(const char* msg,
                       std::vector<ToastEntry>& toasts,
                       ToastType type)
{
    if (!g_toastsEnabled) return;

    // Cap at 4 toasts - drop the oldest if we're at the limit
    if (toasts.size() >= 4)
        toasts.erase(toasts.begin());

    ToastEntry t;
    CONVERSIONS::StrToCharBuf(msg, t.msg, sizeof(t.msg));
    t.type  = type;
    t.timer = g_toastDuration;
    toasts.push_back(t);
}

void RENDER::CenterSpacing(const char* label)
{
    float font_size = (ImGui::GetFontSize() * strlen(label) / 2) + 30.f;
    ImGui::SameLine(
        ImGui::GetWindowSize().x / 2 -
        font_size + (font_size / 2));
}

void RENDER::FieldLabel(const char* txt, ImFont* font, int theme)
{
    ImGui::PushStyleColor(ImGuiCol_Text, THEME::TC(
        ImVec4(0.392f, 0.455f, 0.545f, 1.0f),
        ImVec4(0.320f, 0.370f, 0.440f, 1.0f), theme));
    ImGui::PushFont(font);
    ImGui::SetCursorPosX(12);
    ImGui::TextUnformatted(txt);
    ImGui::PopFont();
    ImGui::PopStyleColor();
}

void RENDER::DangerFieldLabel(const char* txt, ImFont* font, int theme)
{
    ImGui::PushStyleColor(ImGuiCol_Text, 
        ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    ImGui::PushFont(font);
    ImGui::SetCursorPosX(12);
    ImGui::TextUnformatted(txt);
    ImGui::PopFont();
    ImGui::PopStyleColor();
}


bool RENDER::GreenButton(const char* label)
{
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.13f, 0.60f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.17f, 0.74f, 0.44f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.47f, 0.27f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f,  1.0f,  1.0f,  1.0f));
    bool pressed = ImGui::Button(label);
    ImGui::PopStyleColor(4);
    return pressed;
}

bool RENDER::ThemeButton(const char* label, const ImVec2& size_arg)
{
    // Dark mode: purple  /  Light mode: cornflower blue
    ImVec4 base = (g_theme == 0)
        ? ImVec4(0.235f, 0.220f, 0.470f, 1.0f)
        : ImVec4(0.14f,  0.42f,  0.75f,  1.0f);
    ImVec4 hov  = (g_theme == 0)
        ? ImVec4(0.310f, 0.290f, 0.580f, 1.0f)
        : ImVec4(0.18f,  0.50f,  0.84f,  1.0f);
    ImVec4 act  = (g_theme == 0)
        ? ImVec4(0.180f, 0.168f, 0.360f, 1.0f)
        : ImVec4(0.10f,  0.32f,  0.60f,  1.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        base);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hov);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  act);
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    bool pressed = ImGui::Button(label, size_arg);
    ImGui::PopStyleColor(4);
    return pressed;
}

bool RENDER::RedButton(const char* label)
{
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.60f, 0.13f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.74f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.47f, 0.10f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f,  1.0f,  1.0f,  1.0f));
    bool pressed = ImGui::Button(label);
    ImGui::PopStyleColor(4);
    return pressed;
}

bool RENDER::CopyButton(const char* id, const char* textToCopy, int theme,
                        std::vector<ToastEntry>& toasts)
{
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Button, THEME::TC(
        ImVec4(0.145f, 0.162f, 0.245f, 1.0f),
        ImVec4(0.832f, 0.815f, 0.780f, 1.0f), theme));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.235f, 0.220f, 0.470f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.180f, 0.168f, 0.360f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f,   1.0f,   1.0f,   1.0f));
    bool pressed = ImGui::Button("  Copy  ");
    ImGui::PopStyleColor(4);

    if (pressed)
    {
        ImGui::SetClipboardText(textToCopy);
        ShowToast("Copied to clipboard!", toasts, ToastType::Success);
    }

    ImGui::PopID();
    return pressed;
}

void RENDER::DrawStrengthBar(const char* pwd, ImFont* font, int theme)
{
    int    score = PasswordManager::PasswordStrength(pwd);
    float  frac  = (float)score / 5.0f;
    ImVec4 col   = THEME::StrengthColor(score);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, THEME::TC(
        ImVec4(0.10f, 0.11f, 0.18f,  1.0f),
        ImVec4(0.844f, 0.828f, 0.794f, 1.0f), theme));
    ImGui::SetCursorPosX(3);
    ImGui::ProgressBar(frac, ImVec2(-1, 8), "");
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::PushFont(font);
    ImGui::TextUnformatted(PasswordManager::PasswordStrengthLabel(score));
    ImGui::PopFont();
    ImGui::PopStyleColor();
}

// ============================================================
//  RENDER::RenderToasts
//  Draws toasts via GetForegroundDrawList() (always on top) and
//  handles X-dismiss via direct mouse hit-testing on ImGui::GetIO(),
//  avoiding InvisibleButton clipping issues entirely.
// ============================================================
void RENDER::RenderToasts(std::vector<ToastEntry>& toasts, float dt, ImFont* fontSmall)
{
    // Age out expired toasts
    for (auto& t : toasts) t.timer -= dt;
    toasts.erase(std::remove_if(toasts.begin(), toasts.end(),
        [](const ToastEntry& t){ return t.timer <= 0.0f; }), toasts.end());
    if (toasts.empty()) return;

    const float TOAST_W  = 310.f;
    const float TOAST_H  = 52.f;
    const float ICON_W   = 48.f;
    const float GAP      = 8.f;
    const float MARGIN_R = 16.f;
    const float MARGIN_B = 16.f;

    ImGuiIO&    io   = ImGui::GetIO();
    ImDrawList* dl   = ImGui::GetForegroundDrawList();
    ImVec2      mp   = io.MousePos;
    bool        clicked = ImGui::IsMouseClicked(0);

    float baseX = io.DisplaySize.x - TOAST_W - MARGIN_R;
    float baseY = io.DisplaySize.y - MARGIN_B;

    for (int i = (int)toasts.size() - 1; i >= 0; --i)
    {
        auto& t = toasts[i];
        float alpha  = std::min(t.timer, 1.0f);
        ImU8  a      = (ImU8)(alpha * 255);
        ImU8  aBg    = (ImU8)(alpha * 242);

        float toastX = baseX;
        float toastY = baseY - TOAST_H;
        baseY = toastY - GAP;

        ImU32 bgCol, iconBgCol;
        switch (t.type)
        {
        case ToastType::Error:
            bgCol     = IM_COL32(58,  12,  12,  aBg);
            iconBgCol = IM_COL32(220, 38,  38,  a);
            break;
        case ToastType::Warning:
            bgCol     = IM_COL32(58,  34,  6,   aBg);
            iconBgCol = IM_COL32(217, 119, 6,   a);
            break;
        case ToastType::Info:
            bgCol     = IM_COL32(8,   26,  62,  aBg);
            iconBgCol = IM_COL32(59,  130, 246, a);
            break;
        default: // Success
            bgCol     = IM_COL32(10,  46,  24,  aBg);
            iconBgCol = IM_COL32(34,  197, 94,  a);
            break;
        }

        // Background card + subtle border
        dl->AddRectFilled(
            ImVec2(toastX, toastY),
            ImVec2(toastX + TOAST_W, toastY + TOAST_H),
            bgCol, 8.f);
        dl->AddRect(
            ImVec2(toastX, toastY),
            ImVec2(toastX + TOAST_W, toastY + TOAST_H),
            IM_COL32(255, 255, 255, (ImU8)(alpha * 18)), 8.f, 0, 1.f);

        // Coloured icon area (rounded on left side only)
        dl->AddRectFilled(
            ImVec2(toastX, toastY),
            ImVec2(toastX + ICON_W, toastY + TOAST_H),
            iconBgCol, 8.f, ImDrawFlags_RoundCornersLeft);

        // Icon symbol
        ImU32 iconCol = IM_COL32(255, 255, 255, a);
        float cx = toastX + ICON_W * 0.5f;
        float cy = toastY + TOAST_H * 0.5f;
        switch (t.type)
        {
        case ToastType::Success:
            dl->AddLine({cx - 7.f, cy + 0.5f}, {cx - 2.f, cy + 5.5f}, iconCol, 2.2f);
            dl->AddLine({cx - 2.f, cy + 5.5f}, {cx + 7.f, cy - 4.5f}, iconCol, 2.2f);
            break;
        case ToastType::Error:
            dl->AddLine({cx - 6.f, cy - 6.f}, {cx + 6.f, cy + 6.f}, iconCol, 2.2f);
            dl->AddLine({cx + 6.f, cy - 6.f}, {cx - 6.f, cy + 6.f}, iconCol, 2.2f);
            break;
        case ToastType::Warning:
            dl->AddLine({cx, cy - 7.f}, {cx, cy + 0.5f}, iconCol, 2.4f);
            dl->AddCircleFilled({cx, cy + 5.5f}, 2.f, iconCol, 8);
            break;
        case ToastType::Info:
            dl->AddCircleFilled({cx, cy - 7.f}, 2.f, iconCol, 8);
            dl->AddLine({cx, cy - 2.5f}, {cx, cy + 7.f}, iconCol, 2.4f);
            break;
        }

        // Message text
        ImFont* f  = fontSmall ? fontSmall : ImGui::GetFont();
        float   fs = fontSmall ? 13.f : ImGui::GetFontSize();
        dl->AddText(f, fs,
            ImVec2(toastX + ICON_W + 12.f, toastY + (TOAST_H - fs) * 0.5f),
            IM_COL32(235, 235, 235, a), t.msg);

        // Dismiss X - draw it, then check mouse hit manually
        float xCx = toastX + TOAST_W - 14.f;
        float xCy = toastY + TOAST_H * 0.5f;
        bool  hoveringX = (mp.x >= xCx - 9.f && mp.x <= xCx + 9.f &&
                           mp.y >= xCy - 9.f && mp.y <= xCy + 9.f);
        ImU32 xCol = hoveringX
            ? IM_COL32(255, 255, 255, a)
            : IM_COL32(180, 180, 180, (ImU8)(alpha * 160));
        dl->AddLine({xCx - 4.5f, xCy - 4.5f}, {xCx + 4.5f, xCy + 4.5f}, xCol, 1.6f);
        dl->AddLine({xCx + 4.5f, xCy - 4.5f}, {xCx - 4.5f, xCy + 4.5f}, xCol, 1.6f);

        if (hoveringX && clicked)
            t.timer = 0.f;
    }
}
