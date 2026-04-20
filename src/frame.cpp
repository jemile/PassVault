// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  frame.cpp  –  PassVault GUI
//  Requires: OpenGL 3.3, GLFW, Dear ImGui, glad
// ============================================================

// Windows API (NOMINMAX prevents min/max macro conflicts)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <cstring>
#include <cctype>
#include <cmath>

#include "frame.h"
#include "password_manager.h"
#include "crypto.h"
#include "hatten_font.h"
#include "sun_moon_font.h"

// ============================================================
//  FRAME namespace globals
// ============================================================
namespace FRAME
{
    int         width           = 1280;
    int         height          = 720;
    int         menuTab         = 0;
    int         theme           = 0;
	int 	    filterTheme     = -1;  // -1 forces theme application on first frame
    int         autoLockIndex   = 1;
    float       autoLockTimeout = 60.0;
	bool		shouldExit      = false;
    bool        settingsTab     = false;

    GLFWwindow* window          = nullptr;
    ImFont*     font            = nullptr;   // regular  (20 px)
    ImFont*     fontTitle       = nullptr;   // headings (25 px)
    ImFont*     fontSmall       = nullptr;   // captions (16 px)
	ImFont*     sunMoonFontBig  = nullptr;   // icons (32 px)
}

// ============================================================
//  Application Core State
// ============================================================
enum class AppState { Setup, Locked, Vault };
static AppState appState = AppState::Locked;  // refined on first frame
static bool     appStateChecked = false;

// ============================================================
//  Lock Screen State
// ============================================================
static char  lockPwBuf[256] = {};
static char  lockConfirmBuf[256] = {};
static bool  lockShowPw = false;
static bool  lockShowConfirm = false;
static char  lockErrMsg[128] = {};
static bool  lockWorking = false;  // true while Argon2 is running

// ============================================================
//  Auto-Lock & Activity Tracking
// ============================================================
static double lastActivityTime = 0.0;   // seconds (glfwGetTime)

// ============================================================
//  Password Manager Core
// ============================================================
static PasswordManager pm;
static bool            pmInitialized = false;

// ============================================================
//  Vault Browsing State
// ============================================================
static int         selectedIdx = -1;   // index in pm.entries
static bool        showDeleteConfirm = false;
static bool        showPassword = false;
static bool        showGenPopup = false;

// ============================================================
//  Entry Editing State
// ============================================================
static bool        editMode = false;
static bool        isNewEntry = false;
static char        editTitle[128] = {};
static char        editWebsite[256] = {};
static char        editUsername[256] = {};
static char        editPassword[256] = {};
static int         editCatIdx = 0;
static char        editNotes[2048] = {};
static std::string editingId = "";

// ============================================================
//  Sidebar & Search/Filter
// ============================================================
static char searchBuf[256] = {};
static int  filterCatIdx = -1;   // -1 = All

// ============================================================
//  Password Generator
// ============================================================
static int  genLength = 16;
static bool genUpper = true;
static bool genLower = true;
static bool genDigits = true;
static bool genSymbols = true;
static char genPreview[256] = {};

// ============================================================
//  UI Feedback
// ============================================================
static char  toastMsg[128] = {};
static float toastTimer = 0.0f;

// ============================================================
//  Constants
// ============================================================
static const char* CATEGORIES[] = { "Personal", "Work", "Finance", "Social", "Other" };
static const int   NUM_CATS = 5;

static ImVec4 GetCatColor(const std::string& cat)
{
    if (cat == "Personal") return ImVec4(0.310f, 0.760f, 0.970f, 1.0f);  // sky blue
    if (cat == "Work")     return ImVec4(0.980f, 0.750f, 0.140f, 1.0f);  // amber
    if (cat == "Finance")  return ImVec4(0.130f, 0.770f, 0.370f, 1.0f);  // green
    if (cat == "Social")   return ImVec4(0.960f, 0.280f, 0.700f, 1.0f);  // pink
    return                        ImVec4(0.580f, 0.630f, 0.730f, 1.0f);  // slate (Other)
}

// ============================================================
//  Theme helpers – pick dark or light colour at runtime
// ============================================================
static inline ImVec4 TC(ImVec4 dark, ImVec4 light)
{
    return FRAME::theme == 0 ? dark : light;
}
static inline ImU32 TCU(ImU32 dark, ImU32 light)
{
    return FRAME::theme == 0 ? dark : light;
}

// ============================================================
//  Helper – copy a std::string into a char buffer
// ============================================================
static void StrToCharBuf(const std::string& src, char* dst, size_t dstSize)
{
    strncpy_s(dst, dstSize, src.c_str(), _TRUNCATE);
}

static void LoadEntryIntoBuffers(const PasswordEntry& e)
{
    StrToCharBuf(e.title,    editTitle,    sizeof(editTitle));
    StrToCharBuf(e.website,  editWebsite,  sizeof(editWebsite));
    StrToCharBuf(e.username, editUsername, sizeof(editUsername));
    StrToCharBuf(e.password, editPassword, sizeof(editPassword));
    StrToCharBuf(e.notes,    editNotes,    sizeof(editNotes));
    editCatIdx = 0;
    for (int i = 0; i < NUM_CATS; ++i)
        if (e.category == CATEGORIES[i]) { editCatIdx = i; break; }
    editingId    = e.id;
    showPassword = false;
}

static void ClearEditBuffers()
{
    memset(editTitle,    0, sizeof(editTitle));
    memset(editWebsite,  0, sizeof(editWebsite));
    memset(editUsername, 0, sizeof(editUsername));
    memset(editPassword, 0, sizeof(editPassword));
    memset(editNotes,    0, sizeof(editNotes));
    editCatIdx   = 0;
    editingId    = "";
    showPassword = false;
}

static void ShowToast(const char* msg)
{
    StrToCharBuf(msg, toastMsg, sizeof(toastMsg));
    toastTimer = 4.0f;
}

// ============================================================
//  Strength bar colors
// ============================================================
static ImVec4 StrengthColor(int s)
{
    switch (s)
    {
        case 0: return ImVec4(0.86f, 0.20f, 0.20f, 1.0f);
        case 1: return ImVec4(0.96f, 0.45f, 0.10f, 1.0f);
        case 2: return ImVec4(0.96f, 0.80f, 0.10f, 1.0f);
        case 3: return ImVec4(0.56f, 0.90f, 0.30f, 1.0f);
        case 4: return ImVec4(0.20f, 0.80f, 0.40f, 1.0f);
        default:return ImVec4(0.10f, 0.95f, 0.55f, 1.0f);
    }
}

// ============================================================
//  FRAME::SetupImGuiStyle  –  Setup style and load fonts
// ============================================================
inline void FRAME::SetupImGuiStyle()
{
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    io.IniFilename = nullptr;

	// Tell ImGui to keep our font data in memory (we manage it ourselves)
    ImFontConfig cfg = {};
    cfg.FontDataOwnedByAtlas = false;

    // Load three font sizes from the embedded Hatten TTF
    font = io.Fonts->AddFontFromMemoryTTF(
        (void*)HattenFont, sizeof(HattenFont), 20.0f,
        &cfg, io.Fonts->GetGlyphRangesCyrillic());

    fontTitle = io.Fonts->AddFontFromMemoryTTF(
        (void*)HattenFont, sizeof(HattenFont), 25.0f,
        &cfg, io.Fonts->GetGlyphRangesCyrillic());

    fontSmall = io.Fonts->AddFontFromMemoryTTF(
        (void*)HattenFont, sizeof(HattenFont), 16.0f,
        &cfg, io.Fonts->GetGlyphRangesCyrillic());

    sunMoonFontBig = io.Fonts->AddFontFromMemoryTTF(
        (void*)SunMoonFont, sizeof(SunMoonFont), 32.0f,
		&cfg, io.Fonts->GetGlyphRangesCyrillic());

    ImGuiStyle& s = ImGui::GetStyle();

    // Layout
    s.Alpha              = 1.0f;
    s.WindowPadding      = ImVec2(12.0f, 12.0f);
    s.WindowRounding     = 6.0f;
    s.WindowBorderSize   = 1.0f;
    s.ChildRounding      = 6.0f;
    s.ChildBorderSize    = 0.0f;
    s.FramePadding       = ImVec2(8.0f, 5.0f);
    s.FrameRounding      = 5.0f;
    s.FrameBorderSize    = 0.0f;
    s.ItemSpacing        = ImVec2(8.0f, 6.0f);
    s.ItemInnerSpacing   = ImVec2(6.0f, 4.0f);
    s.ScrollbarSize      = 10.0f;
    s.ScrollbarRounding  = 5.0f;
    s.GrabMinSize        = 8.0f;
    s.GrabRounding       = 4.0f;
    s.TabRounding        = 5.0f;
    s.TabBorderSize      = 0.0f;
    s.PopupRounding      = 6.0f;
    s.PopupBorderSize    = 1.0f;
    s.ButtonTextAlign    = ImVec2(0.5f, 0.5f);
    s.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
    s.IndentSpacing      = 16.0f;
}

inline void FRAME::DarkTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();

    ImVec4* c = s.Colors;

    // Background
    c[ImGuiCol_WindowBg] = ImVec4(0.067f, 0.075f, 0.118f, 1.0f);
    c[ImGuiCol_ChildBg] = ImVec4(0.082f, 0.090f, 0.137f, 1.0f);
    c[ImGuiCol_PopupBg] = ImVec4(0.090f, 0.100f, 0.153f, 1.0f);

    // Borders
    c[ImGuiCol_Border] = ImVec4(0.118f, 0.137f, 0.212f, 1.0f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Text
    c[ImGuiCol_Text] = ImVec4(0.886f, 0.902f, 0.941f, 1.0f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.392f, 0.455f, 0.545f, 1.0f);

    // Frames (input fields, combo boxes, etc.)
    c[ImGuiCol_FrameBg] = ImVec4(0.110f, 0.125f, 0.196f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.145f, 0.162f, 0.245f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.175f, 0.195f, 0.290f, 1.0f);

    // Title bar
    c[ImGuiCol_TitleBg] = ImVec4(0.055f, 0.063f, 0.098f, 1.0f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.067f, 0.075f, 0.118f, 1.0f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.055f, 0.063f, 0.098f, 1.0f);

    // Menu bar
    c[ImGuiCol_MenuBarBg] = ImVec4(0.047f, 0.055f, 0.086f, 1.0f);

    // Scrollbar
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.055f, 0.063f, 0.098f, 1.0f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.235f, 0.220f, 0.470f, 1.0f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.310f, 0.290f, 0.580f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.424f, 0.388f, 0.760f, 1.0f);

    // Buttons  (accent purple)
    c[ImGuiCol_Button] = ImVec4(0.235f, 0.220f, 0.470f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.310f, 0.290f, 0.580f, 1.0f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.180f, 0.168f, 0.360f, 1.0f);

    // Headers / selectables
    c[ImGuiCol_Header] = ImVec4(0.235f, 0.220f, 0.470f, 0.7f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.235f, 0.220f, 0.470f, 0.9f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.235f, 0.220f, 0.470f, 1.0f);

    // Separator
    c[ImGuiCol_Separator] = ImVec4(0.118f, 0.137f, 0.212f, 1.0f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.424f, 0.388f, 0.863f, 0.8f);
    c[ImGuiCol_SeparatorActive] = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);

    // Resize grip
    c[ImGuiCol_ResizeGrip] = ImVec4(0.235f, 0.220f, 0.470f, 0.5f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.424f, 0.388f, 0.863f, 0.8f);
    c[ImGuiCol_ResizeGripActive] = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);

    // Tabs
    c[ImGuiCol_Tab] = ImVec4(0.082f, 0.090f, 0.137f, 1.0f);
    c[ImGuiCol_TabHovered] = ImVec4(0.310f, 0.290f, 0.580f, 0.9f);
    c[ImGuiCol_TabActive] = ImVec4(0.235f, 0.220f, 0.470f, 1.0f);
    c[ImGuiCol_TabUnfocused] = ImVec4(0.067f, 0.075f, 0.118f, 1.0f);
    c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.145f, 0.137f, 0.275f, 1.0f);

    // Misc
    c[ImGuiCol_CheckMark] = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.520f, 0.490f, 0.950f, 1.0f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.235f, 0.220f, 0.470f, 0.6f);
    c[ImGuiCol_DragDropTarget] = ImVec4(0.424f, 0.388f, 0.863f, 0.9f);
    c[ImGuiCol_NavHighlight] = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.6f);
    c[ImGuiCol_PlotHistogram] = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.520f, 0.490f, 0.950f, 1.0f);
}

inline void FRAME::LightTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();

    ImVec4* c = s.Colors;

    // === NEW LIGHT THEME  ===
    
    // Background
    c[ImGuiCol_WindowBg] = ImVec4(0.960f, 0.948f, 0.922f, 1.0f);
    c[ImGuiCol_ChildBg] = ImVec4(0.970f, 0.958f, 0.934f, 1.0f);
    c[ImGuiCol_PopupBg] = ImVec4(0.970f, 0.958f, 0.934f, 1.0f);

    // Borders
    c[ImGuiCol_Border] = ImVec4(0.712f, 0.692f, 0.648f, 1.0f);
    c[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    // Text
    c[ImGuiCol_Text] = ImVec4(0.100f, 0.105f, 0.120f, 1.0f);
    c[ImGuiCol_TextDisabled] = ImVec4(0.500f, 0.520f, 0.550f, 1.0f);

    // Frames
    c[ImGuiCol_FrameBg] = ImVec4(0.868f, 0.852f, 0.820f, 1.0f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.944f, 0.930f, 0.904f, 1.0f);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.950f, 0.938f, 0.912f, 1.0f);

    // Title bar
    c[ImGuiCol_TitleBg] = ImVec4(0.944f, 0.930f, 0.904f, 1.0f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.960f, 0.948f, 0.922f, 1.0f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.944f, 0.930f, 0.904f, 1.0f);

    // Menu bar
    c[ImGuiCol_MenuBarBg] = ImVec4(0.950f, 0.938f, 0.912f, 1.0f);

    // Scrollbar
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.944f, 0.930f, 0.904f, 1.0f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.235f, 0.220f, 0.470f, 1.0f);  // same purple
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.310f, 0.290f, 0.580f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.424f, 0.388f, 0.760f, 1.0f);

    // Buttons (accent purple — stays the same, pops on light)
    c[ImGuiCol_Button] = ImVec4(0.235f, 0.220f, 0.470f, 1.0f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.310f, 0.290f, 0.580f, 1.0f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.180f, 0.168f, 0.360f, 1.0f);

    // Headers / selectables
    c[ImGuiCol_Header] = ImVec4(0.235f, 0.220f, 0.470f, 0.7f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.235f, 0.220f, 0.470f, 0.9f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.235f, 0.220f, 0.470f, 1.0f);

    // Separator
    c[ImGuiCol_Separator] = ImVec4(0.712f, 0.692f, 0.648f, 1.0f);
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.424f, 0.388f, 0.863f, 0.8f);
    c[ImGuiCol_SeparatorActive] = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);

    // Resize grip
    c[ImGuiCol_ResizeGrip] = ImVec4(0.235f, 0.220f, 0.470f, 0.5f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.424f, 0.388f, 0.863f, 0.8f);
    c[ImGuiCol_ResizeGripActive] = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);

    // Tabs
    c[ImGuiCol_Tab] = ImVec4(0.960f, 0.948f, 0.922f, 1.0f);
    c[ImGuiCol_TabHovered] = ImVec4(0.310f, 0.290f, 0.580f, 0.9f);
    c[ImGuiCol_TabActive] = ImVec4(0.235f, 0.220f, 0.470f, 1.0f);
    c[ImGuiCol_TabUnfocused] = ImVec4(0.944f, 0.930f, 0.904f, 1.0f);
    c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.950f, 0.938f, 0.912f, 1.0f);

    // Misc
    c[ImGuiCol_CheckMark] = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);
    c[ImGuiCol_SliderGrab] = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.520f, 0.490f, 0.950f, 1.0f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.235f, 0.220f, 0.470f, 0.6f);
    c[ImGuiCol_DragDropTarget] = ImVec4(0.424f, 0.388f, 0.863f, 0.9f);
    c[ImGuiCol_NavHighlight] = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.4f);
    c[ImGuiCol_PlotHistogram] = ImVec4(0.424f, 0.388f, 0.863f, 1.0f);
    c[ImGuiCol_PlotHistogramHovered] = ImVec4(0.520f, 0.490f, 0.950f, 1.0f);
}

// ============================================================
//  FRAME::InitGlfwFlags
// ============================================================
inline void FRAME::InitGlfwFlags()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
}

// ============================================================
//  FRAME::CenterSpacing
// ============================================================
inline void FRAME::CenterSpacing(const char* label)
{
    float font_size = (ImGui::GetFontSize() * std::string(label).size() / 2) + 30.f;
    ImGui::SameLine(
        ImGui::GetWindowSize().x / 2 -
        font_size + (font_size / 2));
}

// ============================================================
//  Small utility: styled "label" heading text
// ============================================================
static void FieldLabel(const char* txt)
{
    ImGui::PushStyleColor(ImGuiCol_Text, TC(ImVec4(0.392f, 0.455f, 0.545f, 1.0f),
                                             ImVec4(0.320f, 0.370f, 0.440f, 1.0f)));
    ImGui::PushFont(FRAME::fontSmall);
    ImGui::SetCursorPosX(12);
    ImGui::TextUnformatted(txt);
    ImGui::PopFont();
    ImGui::PopStyleColor();
}

// Colored accent button (success green)
static bool GreenButton(const char* label)
{
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.13f, 0.60f, 0.35f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.17f, 0.74f, 0.44f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.10f, 0.47f, 0.27f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    bool pressed = ImGui::Button(label);
    ImGui::PopStyleColor(4);
    return pressed;
}

// Colored accent button (neutral grey)
static bool PurpleButton(const char* label, const ImVec2& size_arg)
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.235f, 0.220f, 0.470f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.310f, 0.290f, 0.580f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.180f, 0.168f, 0.360f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    bool pressed = ImGui::Button(label, size_arg);

    ImGui::PopStyleColor(4);
    return pressed;
}

// Colored accent button (danger red)
static bool RedButton(const char* label)
{
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.60f, 0.13f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.74f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.47f, 0.10f, 0.10f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    bool pressed = ImGui::Button(label);
    ImGui::PopStyleColor(4);
    return pressed;
}

// Small copy button
static bool CopyButton(const char* id, const char* textToCopy)
{
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Button,        TC(ImVec4(0.145f, 0.162f, 0.245f, 1.0f),
                                                     ImVec4(0.832f, 0.815f, 0.780f, 1.0f)));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.235f, 0.220f, 0.470f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.180f, 0.168f, 0.360f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    bool pressed = ImGui::Button("  Copy  ");
    ImGui::PopStyleColor(4);
    if (pressed)
    {
        ImGui::SetClipboardText(textToCopy);
        ShowToast("Copied to clipboard!");
    }
    ImGui::PopID();
    return pressed;
}

// ============================================================
//  Password strength bar (inline)
// ============================================================
static void DrawStrengthBar(const char* pwd)
{
    int   s     = PasswordManager::PasswordStrength(pwd);
    float frac  = (float)s / 5.0f;
    ImVec4 col  = StrengthColor(s);

    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, TC(ImVec4(0.10f, 0.11f, 0.18f, 1.0f),
                                               ImVec4(0.844f, 0.828f, 0.794f, 1.0f)));
    ImGui::ProgressBar(frac, ImVec2(-1, 8), "");
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::PushFont(FRAME::fontSmall);
    ImGui::TextUnformatted(PasswordManager::PasswordStrengthLabel(s));
    ImGui::PopFont();
    ImGui::PopStyleColor();
}

// ============================================================
//  Password Generator Popup
// ============================================================
static void RenderGenPopup()
{
    if (!showGenPopup) return;

    ImGui::OpenPopup("Password Generator");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 300));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, TC(ImVec4(0.082f, 0.090f, 0.137f, 1.0f),
                                               ImVec4(0.970f, 0.958f, 0.934f, 1.0f)));

    if (ImGui::BeginPopupModal("Password Generator", &showGenPopup,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::PushFont(FRAME::fontTitle);
        ImGui::TextColored(ImVec4(0.66f, 0.62f, 1.0f, 1.0f), "Password Generator");
        ImGui::PopFont();
        ImGui::Separator();
        ImGui::Spacing();

        // Length slider
        FieldLabel("LENGTH");
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##genlen", &genLength, 8, 64);
        ImGui::Spacing();

        // Character set toggles
        FieldLabel("CHARACTER SETS");
        ImGui::Checkbox("Uppercase (A-Z)",   &genUpper);
        ImGui::SameLine(210);
        ImGui::Checkbox("Lowercase (a-z)",   &genLower);
        ImGui::Checkbox("Numbers  (0-9)",    &genDigits);
        ImGui::SameLine(210);
        ImGui::Checkbox("Symbols  (!@#...)", &genSymbols);
        ImGui::Spacing();

        // Preview field
        FieldLabel("PREVIEW");
        ImGui::PushStyleColor(ImGuiCol_FrameBg, TC(ImVec4(0.055f, 0.063f, 0.098f, 1.0f),
                                               ImVec4(0.868f, 0.852f, 0.820f, 1.0f)));
        ImGui::SetNextItemWidth(-80);
        ImGui::InputText("##genprev", genPreview, sizeof(genPreview),
            ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor();

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        if (ImGui::Button("Roll", ImVec2(70, 0)))
        {
            std::string pw = PasswordManager::GeneratePassword(
                genLength, genUpper, genLower, genDigits, genSymbols);
            StrToCharBuf(pw, genPreview, sizeof(genPreview));
        }
		ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (GreenButton("  Use This Password  "))
        {
            StrToCharBuf(genPreview, editPassword, sizeof(editPassword));
            showGenPopup = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        if (ImGui::Button("  Cancel  "))
        {
            showGenPopup = false;
            ImGui::CloseCurrentPopup();
        }
		ImGui::PopStyleColor();

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

// ============================================================
//  Delete Confirmation Popup
// ============================================================
static void RenderDeleteConfirmPopup()
{
    if (!showDeleteConfirm) return;

    ImGui::OpenPopup("Confirm Delete");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(340, 208));

    ImGui::PushStyleColor(ImGuiCol_PopupBg, TC(ImVec4(0.082f, 0.090f, 0.137f, 1.0f),
                                               ImVec4(0.970f, 0.958f, 0.934f, 1.0f)));

    if (ImGui::BeginPopupModal("Confirm Delete", &showDeleteConfirm,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Spacing();
        ImGui::PushFont(FRAME::fontTitle);
        ImGui::TextColored(ImVec4(0.96f, 0.30f, 0.30f, 1.0f), "Delete Entry?");
        ImGui::PopFont();
        ImGui::Spacing();

        if (selectedIdx >= 0 && selectedIdx < (int)pm.entries.size())
        {
            ImGui::TextWrapped("Are you sure you want to delete \"%s\"? This cannot be undone.",
                pm.entries[selectedIdx].title.c_str());
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (RedButton("  Yes, Delete  "))
        {
            if (selectedIdx >= 0 && selectedIdx < (int)pm.entries.size())
                pm.RemoveEntry(pm.entries[selectedIdx].id);

            selectedIdx      = -1;
            editMode         = false;
            isNewEntry       = false;
            showDeleteConfirm = false;
            ShowToast("Entry deleted.");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("  Cancel  "))
        {
            showDeleteConfirm = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleColor();
}

// ============================================================
//  Sidebar
// ============================================================
static void RenderSidebar(const std::vector<int>& filtered)
{
    using namespace FRAME;

    const float W = ImGui::GetContentRegionAvail().x;

    ImGui::Spacing();

    // --- Search ---
    FieldLabel("SEARCH");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, TC(ImVec4(0.055f, 0.063f, 0.098f, 1.0f),
                                               ImVec4(0.868f, 0.852f, 0.820f, 1.0f)));
    ImGui::SetNextItemWidth(W);
    ImGui::InputTextWithHint("##search", "Search entries...", searchBuf, sizeof(searchBuf));
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // --- Category filter ---
    FieldLabel("FILTER BY CATEGORY");
    ImGui::Spacing();

    ImGui::SetCursorPosX(5);
    // "All" button
    {
        bool active = (filterCatIdx == -1);
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,    ImVec4(0.235f, 0.220f, 0.470f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,      ImVec4(1.0f,   1.0f,   1.0f,   1.0f));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button,    TC(ImVec4(0.110f, 0.125f, 0.196f, 1.0f),
                                                          ImVec4(0.858f, 0.842f, 0.808f, 1.0f)));
            ImGui::PushStyleColor(ImGuiCol_Text,      TC(ImVec4(0.65f,  0.70f,  0.80f,  1.0f),
                                                          ImVec4(0.30f,  0.34f,  0.44f,  1.0f)));
        }
        if (ImGui::Button("All")) filterCatIdx = -1;
        ImGui::PopStyleColor(2);
    }

    // One button per category on the same line (wrapping via NewLine if needed)
    for (int i = 0; i < NUM_CATS; ++i)
    {
        ImGui::SameLine(0, 4);
        bool active = (filterCatIdx == i);
        ImVec4 col  = GetCatColor(CATEGORIES[i]);

        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,    ImVec4(col.x * 0.6f, col.y * 0.6f, col.z * 0.6f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text,      ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button,    TC(ImVec4(0.110f, 0.125f, 0.196f, 1.0f),
                                                          ImVec4(0.858f, 0.842f, 0.808f, 1.0f)));
            ImGui::PushStyleColor(ImGuiCol_Text,      col);
        }
        ImGui::PushID(i + 100);
        if (ImGui::Button(CATEGORIES[i]))
            filterCatIdx = (filterCatIdx == i) ? -1 : i;
        ImGui::PopID();
        ImGui::PopStyleColor(2);
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, TC(ImVec4(0.118f, 0.137f, 0.212f, 1.0f),
                                                  ImVec4(0.712f, 0.692f, 0.648f, 1.0f)));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // --- Entry list ---
    float listH = ImGui::GetContentRegionAvail().y - 52.0f; // leave room for footer
    ImGui::BeginChild("##entrylist", ImVec2(W, listH), false);

    if (filtered.empty())
    {
        ImGui::Spacing();
        float entriesFoundW = ImGui::CalcTextSize("No entries found.").x;
        ImGui::SetCursorPosX((W - entriesFoundW) * 0.5f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.40f, 0.50f, 1.0f));
        ImGui::TextUnformatted("No entries found.");
        ImGui::PopStyleColor();
    }
    else
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();

        for (int idx : filtered)
        {
            const PasswordEntry& e = pm.entries[idx];
            bool selected          = (idx == selectedIdx);

            ImGui::PushID(idx);

            ImVec2 screenPos = ImGui::GetCursorScreenPos();
            float  itemH     = 56.0f;

            // Invisible button for interaction
            if (ImGui::InvisibleButton("##item", ImVec2(W, itemH)))
            {
                selectedIdx  = idx;
                editMode     = false;
                isNewEntry   = false;
                showPassword = false;
            }

            bool hovered = ImGui::IsItemHovered();

            // Background
            ImU32 bgCol;
            if (selected)
                bgCol = IM_COL32(60, 56, 122, 220);
            else if (hovered)
                bgCol = TCU(IM_COL32(28, 32, 50, 200), IM_COL32(210, 198, 178, 200));
            else
                bgCol = IM_COL32(0, 0, 0, 0);

            dl->AddRectFilled(screenPos,
                ImVec2(screenPos.x + W, screenPos.y + itemH),
                bgCol, 6.0f);

            // Category colour dot
            ImVec4 catC4 = GetCatColor(e.category);
            ImU32  catU  = ImGui::ColorConvertFloat4ToU32(catC4);
            dl->AddCircleFilled(
                ImVec2(screenPos.x + 16, screenPos.y + itemH * 0.40f),
                5.0f, catU, 16);

            // Title
            ImVec4 titleCol = selected
                ? ImVec4(1.0f, 1.0f, 1.0f, 1.0f)
                : TC(ImVec4(0.886f, 0.902f, 0.941f, 1.0f), ImVec4(0.100f, 0.105f, 0.120f, 1.0f));
            ImGui::SetCursorScreenPos(ImVec2(screenPos.x + 30, screenPos.y + 9));
            ImGui::PushStyleColor(ImGuiCol_Text, titleCol);
            std::string title = e.title.empty() ? "(untitled)" : e.title;
            if (title.size() > 26) title = title.substr(0, 24) + "..";
            ImGui::TextUnformatted(title.c_str());
            ImGui::PopStyleColor();

            // Website  (dimmer, small font)
            ImGui::SetCursorScreenPos(ImVec2(screenPos.x + 30, screenPos.y + 32));
            ImGui::PushFont(FRAME::fontSmall);
            ImGui::PushStyleColor(ImGuiCol_Text,
                selected ? ImVec4(0.75f, 0.72f, 1.0f, 1.0f)
                         : ImVec4(0.39f, 0.45f, 0.55f, 1.0f));
            std::string site = e.website.empty() ? "" : e.website;
            // Strip protocol prefix
            for (const char* pre : { "https://", "http://", "www." })
            {
                if (site.rfind(pre, 0) == 0) { site = site.substr(strlen(pre)); break; }
            }
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
    ImGui::PushStyleColor(ImGuiCol_Separator, TC(ImVec4(0.118f, 0.137f, 0.212f, 1.0f),
                                                  ImVec4(0.712f, 0.692f, 0.648f, 1.0f)));
    ImGui::Separator();
    ImGui::PopStyleColor();

    float footerY = ImGui::GetCursorPosY() + 4;
    ImGui::SetCursorPosY(footerY);

    ImGui::SetCursorPosX(5);
    if (GreenButton("  +  Add Entry  "))
    {
        ClearEditBuffers();
        editMode   = true;
        isNewEntry = true;
        selectedIdx = -1;
    }

    ImGui::SameLine();

    // Entry count
    ImGui::PushFont(FRAME::fontSmall);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.39f, 0.45f, 0.55f, 1.0f));
    char countBuf[64];
    snprintf(countBuf, sizeof(countBuf), "%d stored", (int)pm.entries.size());
    float textW = ImGui::CalcTextSize(countBuf).x;
    ImGui::SetCursorPosX(W - textW - 4);
    ImGui::TextUnformatted(countBuf);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

// ============================================================
//  Detail Panel  –  welcome / view / edit
// ============================================================
static void RenderDetailPanel()
{
    using namespace FRAME;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));

    // ---- Welcome / empty state ----
    if (selectedIdx < 0 && !editMode)
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
    if (editMode)
    {
        const float W = ImGui::GetContentRegionAvail().x;
        float pwW = W - 20;
        float pwW2 = W - 140;

        ImGui::PushFont(fontTitle);
        ImGui::SetCursorPosX(5);
        ImGui::SetCursorPosY(5);
        ImGui::TextColored(ImVec4(0.65f, 0.61f, 1.0f, 1.0f),
            isNewEntry ? "New Entry" : "Edit Entry");
        ImGui::PopFont();
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Title
        FieldLabel("TITLE");
        ImGui::SetNextItemWidth(W);
        ImGui::SetCursorPosX(2);
        ImGui::SetNextItemWidth(pwW);
        ImGui::InputText("##etitle", editTitle, sizeof(editTitle));
        ImGui::Spacing();

        // Website
        FieldLabel("WEBSITE");
        ImGui::SetNextItemWidth(W);
        ImGui::SetCursorPosX(2);
        ImGui::SetNextItemWidth(pwW);
        ImGui::InputText("##ewebsite", editWebsite, sizeof(editWebsite));
        ImGui::Spacing();

        // Username
        FieldLabel("USERNAME / EMAIL");
        ImGui::SetNextItemWidth(W);
        ImGui::SetCursorPosX(2);
        ImGui::SetNextItemWidth(pwW);
        ImGui::InputText("##euser", editUsername, sizeof(editUsername));
        ImGui::Spacing();

        // Password row
        FieldLabel("PASSWORD");
        ImGui::SetNextItemWidth(pwW2);
        ImGuiInputTextFlags pwFlags = ImGuiInputTextFlags_None;
        if (!showPassword) pwFlags |= ImGuiInputTextFlags_Password;
        ImGui::SetCursorPosX(2);
        ImGui::InputText("##epw", editPassword, sizeof(editPassword), pwFlags);

        ImGui::SameLine(0, 6);

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        if (ImGui::Button(showPassword ? " Hide " : " Show "))
            showPassword = !showPassword;

        ImGui::SameLine(0, 6);
        if (ImGui::Button("  Gen  "))
        {
            // Seed the preview on first open
            if (strlen(genPreview) == 0)
            {
                std::string pw = PasswordManager::GeneratePassword(
                    genLength, genUpper, genLower, genDigits, genSymbols);
                StrToCharBuf(pw, genPreview, sizeof(genPreview));
            }
            showGenPopup = true;
        }

		ImGui::PopStyleColor();

        ImGui::Spacing();

        // Strength bar
        if (strlen(editPassword) > 0)
            DrawStrengthBar(editPassword);
        ImGui::Spacing();

        // Category
        FieldLabel("CATEGORY");
        ImGui::SetNextItemWidth(W);
        ImGui::SetCursorPosX(2);
        ImGui::SetNextItemWidth(pwW);
        ImGui::Combo("##ecat", &editCatIdx, CATEGORIES, NUM_CATS);
        ImGui::Spacing();

        // Notes
        FieldLabel("NOTES");
        ImGui::SetNextItemWidth(W);
        ImGui::SetCursorPosX(2);
        ImGui::SetNextItemWidth(pwW);
        ImGui::InputTextMultiline("##enotes", editNotes, sizeof(editNotes),
            ImVec2(pwW, 90));
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Action buttons
        ImGui::SetCursorPosX(5);
        if (GreenButton(isNewEntry ? "  Save Entry  " : "  Save Changes  "))
        {
            PasswordEntry e{};
            e.id       = editingId;
            e.title    = editTitle;
            e.website  = editWebsite;
            e.username = editUsername;
            e.password = editPassword;
            e.category = CATEGORIES[editCatIdx];
            e.notes    = editNotes;

            if (isNewEntry)
            {
                pm.AddEntry(e);
                selectedIdx = (int)pm.entries.size() - 1;
                ShowToast("Entry saved!");
            }
            else
            {
                pm.UpdateEntry(e);
                selectedIdx = pm.FindIndexById(e.id);
                ShowToast("Changes saved!");
            }

            editMode   = false;
            isNewEntry = false;
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

        if (ImGui::Button("  Cancel  "))
        {
            editMode   = false;
            isNewEntry = false;
            if (isNewEntry) selectedIdx = -1;
        }

		ImGui::PopStyleColor();
        ImGui::PopStyleVar();
        return;
    }

    // ---- View mode ----
    if (selectedIdx < 0 || selectedIdx >= (int)pm.entries.size())
    {
        ImGui::PopStyleVar();
        return;
    }

    const PasswordEntry& e = pm.entries[selectedIdx];
    const float W          = ImGui::GetContentRegionAvail().x;
    float pwW = W - 20;


    // Header: title + category badge
    ImGui::PushFont(fontTitle);
    ImGui::SetCursorPosX(5);
    ImGui::SetCursorPosY(6);
    ImGui::TextColored(TC(ImVec4(0.886f, 0.902f, 0.941f, 1.0f),
                          ImVec4(0.100f, 0.105f, 0.120f, 1.0f)),
        e.title.empty() ? "(untitled)" : e.title.c_str());
    ImGui::PopFont();

    // Category badge
    ImVec4 catCol = GetCatColor(e.category);
    ImGui::SameLine(0, 12);
    ImGui::PushStyleColor(ImGuiCol_Text,   catCol);
    ImGui::PushFont(fontSmall);
    std::string badge = e.category.empty() ? "Other" : e.category;
    ImGui::SetCursorPosY(6);
    ImGui::TextUnformatted(badge.c_str());
    ImGui::PopFont();
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Website
    if (!e.website.empty())
    {
        FieldLabel("WEBSITE");
        ImGui::SetCursorPosX(10);
        ImGui::TextColored(ImVec4(0.45f, 0.72f, 1.0f, 1.0f), "%s", e.website.c_str());
        ImGui::Spacing();
    }

    // Username
    FieldLabel("USERNAME / EMAIL");
    ImGui::SetNextItemWidth(W - 75);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, TC(ImVec4(0.055f, 0.063f, 0.098f, 1.0f),
                                               ImVec4(0.868f, 0.852f, 0.820f, 1.0f)));
    ImGui::SetCursorPosX(2);
    ImGui::InputText("##vuser", (char*)e.username.c_str(), e.username.size() + 1,
        ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 8);
    CopyButton("cpyuser", e.username.c_str());
    ImGui::Spacing();

    // Password
    FieldLabel("PASSWORD");
    ImGui::SetNextItemWidth(W - 135);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, TC(ImVec4(0.055f, 0.063f, 0.098f, 1.0f),
                                               ImVec4(0.868f, 0.852f, 0.820f, 1.0f)));
    ImGuiInputTextFlags vPwFlags = ImGuiInputTextFlags_ReadOnly;
    if (!showPassword) vPwFlags |= ImGuiInputTextFlags_Password;
    ImGui::SetCursorPosX(2);
    ImGui::InputText("##vpw", (char*)e.password.c_str(), e.password.size() + 1, vPwFlags);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 6);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if (ImGui::Button(showPassword ? " Hide " : " Show "))
        showPassword = !showPassword;
	ImGui::PopStyleColor(); 
    ImGui::SameLine(0, 6);
    CopyButton("cpypw", e.password.c_str());

    // Strength
    ImGui::Spacing();
    if (!e.password.empty())
    {
        FieldLabel("STRENGTH");
        DrawStrengthBar(e.password.c_str());
    }
    ImGui::Spacing();

    // Notes
    if (!e.notes.empty())
    {
        FieldLabel("NOTES");
        ImGui::PushStyleColor(ImGuiCol_FrameBg, TC(ImVec4(0.055f, 0.063f, 0.098f, 1.0f),
                                               ImVec4(0.868f, 0.852f, 0.820f, 1.0f)));
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
    ImGui::Text("Modified: %s", e.modifiedAt.c_str());
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
        LoadEntryIntoBuffers(e);
        editMode   = true;
        isNewEntry = false;
    }
	ImGui::PopStyleColor();
    ImGui::SameLine();
    if (RedButton("  Delete  "))
        showDeleteConfirm = true;

    ImGui::PopStyleVar();
}

// ============================================================
//  RenderSettingsScreen  –  User Customization and App Info
// ============================================================

static void FRAME::RenderSettingsScreen()
{
    const float TH = 40.0f;
    const float cardW = 600.0f;
    const float cardH = 500.0f;
    const float contentW = (float)width;
    const float contentH = (float)height - TH;
    const float cardX = (contentW - cardW) * 0.5f;
    const float cardY = (contentH - cardH) * 0.42f; // slightly above center
    const float bottomOfCard = cardH * 0.9f;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2      winSP = ImGui::GetWindowPos();

    // ---- Card shadow ----
    ImVec2 csp(winSP.x + cardX, winSP.y + TH + cardY);
    dl->AddRectFilled(
        ImVec2(csp.x + 5, csp.y + 6),
        ImVec2(csp.x + cardW + 5, csp.y + cardH + 6),
        TCU(IM_COL32(0, 0, 0, 70), IM_COL32(0, 0, 0, 28)), 14.0f);

    // ---- Card body ----
    dl->AddRectFilled(csp, ImVec2(csp.x + cardW, csp.y + cardH),
        TCU(IM_COL32(22, 20, 46, 255), IM_COL32(250, 246, 238, 255)), 14.0f);
    dl->AddRect(csp, ImVec2(csp.x + cardW, csp.y + cardH),
        TCU(IM_COL32(80, 72, 160, 200), IM_COL32(185, 172, 148, 200)), 14.0f, 0, 1.5f);

    // ---- Widget content via transparent child window ----
    ImGui::SetCursorPos(ImVec2(cardX, TH + cardY));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32, 24));
    ImGui::BeginChild("##settingscard", ImVec2(cardW, cardH), false,
        ImGuiWindowFlags_NoScrollbar);

    const float innerW = cardW - 64.0f;  // padded width

    // ---- Logo ----
    ImGui::PushFont(fontTitle);
    ImVec4 accentCol = ImVec4(0.66f, 0.62f, 1.0f, 1.0f);
    float logoW = ImGui::CalcTextSize("PassVault Settings").x;
    ImGui::SetCursorPosX((innerW - logoW) * 0.5f + 32);
    ImGui::SetCursorPosY(5);
    ImGui::TextColored(accentCol, "PassVault Settings");
    ImGui::PopFont();

    // ---- Subtitle ----
    ImGui::Spacing();
    const char* sub = "User Customization and App Info";
    ImGui::PushFont(fontSmall);
    float subW = ImGui::CalcTextSize(sub).x;
    if (subW > innerW) subW = innerW;
    ImGui::SetCursorPosX((innerW - std::min(subW, innerW)) * 0.5f + 32);
    ImGui::PushStyleColor(ImGuiCol_Text, TC(ImVec4(0.55f, 0.58f, 0.70f, 1.0f),
        ImVec4(0.42f, 0.40f, 0.36f, 1.0f)));
    ImGui::TextWrapped("%s", sub);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator,
        TC(ImVec4(0.16f, 0.15f, 0.32f, 1.0f), ImVec4(0.80f, 0.76f, 0.68f, 1.0f)));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing(); ImGui::Spacing();

    // ---- Settings Area ---- (your widgets go here)
    FieldLabel("AUTO-LOCK AFTER INACTIVITY");

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
        // Instantly update the real timeout
        const float timeouts[] = { 30.0f, 60.0f, 120.0f, 180.0f, -1 }; // 0 = never
        autoLockTimeout = timeouts[autoLockIndex];
    }

    // ---- Return button ----
    const char* homeBtn = "Go Back";

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.235f, 0.220f, 0.470f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.310f, 0.290f, 0.580f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.180f, 0.168f, 0.360f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    float btnWidth = ImGui::CalcTextSize(homeBtn).x * 6.0f;

    ImGui::SetCursorPosX(32.0f + (innerW - btnWidth) * 0.5f);

    ImGui::SetCursorPosY(bottomOfCard);

    bool pressed = ImGui::Button(homeBtn, ImVec2(btnWidth, 0));
    ImGui::PopStyleColor(4);

    if (pressed)
    {
        settingsTab = false;
    }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================
//  RenderLockScreen  –  Setup (first run) or Unlock
// ============================================================
static void FRAME::RenderLockScreen()
{

    const bool isSetup   = (appState == AppState::Setup);
    const float TH       = 40.0f;
    const float cardW    = 420.0f;
    const float cardH    = isSetup ? 358.0f : 298.0f;
    const float contentW = (float)width;
    const float contentH = (float)height - TH;
    const float cardX    = (contentW - cardW) * 0.5f;
    const float cardY    = (contentH - cardH) * 0.42f;  // slightly above center

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    ImVec2      winSP = ImGui::GetWindowPos();

    // ---- Card shadow ----
    ImVec2 csp(winSP.x + cardX, winSP.y + TH + cardY);
    dl->AddRectFilled(
        ImVec2(csp.x + 5, csp.y + 6),
        ImVec2(csp.x + cardW + 5, csp.y + cardH + 6),
        TCU(IM_COL32(0, 0, 0, 70), IM_COL32(0, 0, 0, 28)), 14.0f);

    // ---- Card body ----
    dl->AddRectFilled(csp, ImVec2(csp.x + cardW, csp.y + cardH),
        TCU(IM_COL32(22, 20, 46, 255), IM_COL32(250, 246, 238, 255)), 14.0f);
    dl->AddRect(csp, ImVec2(csp.x + cardW, csp.y + cardH),
        TCU(IM_COL32(80, 72, 160, 200), IM_COL32(185, 172, 148, 200)), 14.0f, 0, 1.5f);

    // ---- Widget content via transparent child window ----
    ImGui::SetCursorPos(ImVec2(cardX, TH + cardY));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(32, 24));
    ImGui::BeginChild("##lockcard", ImVec2(cardW, cardH), false,
        ImGuiWindowFlags_NoScrollbar);

    const float innerW = cardW - 64.0f;  // padded width
	const float bottomOfCard = cardH * 0.75f;
	const float middleOfCard = innerW * 0.09f;

    // ---- Logo ----
    ImGui::PushFont(fontTitle);
    ImVec4 accentCol = ImVec4(0.66f, 0.62f, 1.0f, 1.0f);
    float logoW = ImGui::CalcTextSize("PassVault").x;
    ImGui::SetCursorPosX((innerW - logoW) * 0.5f + 32);
    ImGui::SetCursorPosY(5);
    ImGui::TextColored(accentCol, "PassVault");
    ImGui::PopFont();

    // ---- Subtitle ----
    ImGui::Spacing();
    const char* sub = isSetup
        ? "Create your master password to get started."
        : "Enter your master password to unlock your vault.";
    ImGui::PushFont(fontSmall);
    float subW = ImGui::CalcTextSize(sub).x;
    // Wrap if too wide
    if (subW > innerW) subW = innerW;
    ImGui::SetCursorPosX((innerW - std::min(subW, innerW)) * 0.5f + 32);
    ImGui::PushStyleColor(ImGuiCol_Text, TC(ImVec4(0.55f, 0.58f, 0.70f, 1.0f),
                                             ImVec4(0.42f, 0.40f, 0.36f, 1.0f)));
    ImGui::TextWrapped("%s", sub);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator,
        TC(ImVec4(0.16f, 0.15f, 0.32f, 1.0f), ImVec4(0.80f, 0.76f, 0.68f, 1.0f)));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing(); ImGui::Spacing();

    // ---- Password field ----
    FieldLabel("MASTER PASSWORD");
    float pwFieldW = innerW - 8.0f;
    ImGui::SetNextItemWidth(pwFieldW);
    ImGuiInputTextFlags pwFlags = ImGuiInputTextFlags_None;
    if (!lockShowPw) pwFlags |= ImGuiInputTextFlags_Password;
	ImGui::SetCursorPosX(5);
    ImGui::InputText("##lockpw", lockPwBuf, sizeof(lockPwBuf), pwFlags);
    ImGui::SameLine(0, 6);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if (ImGui::Button(lockShowPw ? " Hide##cc " : " Show##cc "))
        lockShowPw = !lockShowPw;
    ImGui::PopStyleColor();

    if (isSetup)
    {
        ImGui::Spacing();
        FieldLabel("CONFIRM PASSWORD");
        ImGui::SetNextItemWidth(pwFieldW);
        ImGuiInputTextFlags cfFlags = ImGuiInputTextFlags_None;
        if (!lockShowConfirm) cfFlags |= ImGuiInputTextFlags_Password;
        ImGui::SetCursorPosX(5);
        ImGui::InputText("##lockconfirm", lockConfirmBuf, sizeof(lockConfirmBuf), cfFlags);
        ImGui::SameLine(0, 6);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        if (ImGui::Button(lockShowConfirm ? " Hide##xx " : " Show##xxx "))
            lockShowConfirm = !lockShowConfirm;
        ImGui::PopStyleColor();
    }

    ImGui::Spacing(); ImGui::Spacing();

    // ---- Error message ----
    if (lockErrMsg[0] != '\0')
    {
        ImGui::PushFont(fontSmall);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.30f, 0.30f, 1.0f));
        float errW = ImGui::CalcTextSize(lockErrMsg).x;
        ImGui::SetCursorPosX((innerW - errW) * 0.5f + 32);
        ImGui::TextUnformatted(lockErrMsg);
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::Spacing();
    }

    // ---- Action button ----
    const char* btnLabel = lockWorking
        ? "  Working...  "
        : (isSetup ? "  Create Vault  " : "  Unlock  ");

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.235f, 0.220f, 0.470f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.310f, 0.290f, 0.580f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.180f, 0.168f, 0.360f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

    float btnWidth = ImGui::CalcTextSize(btnLabel).x * 6.0f;

    ImGui::SetCursorPosX(32.0f + (innerW - btnWidth) * 0.5f);

    ImGui::SetCursorPosY(bottomOfCard);

    bool pressed = ImGui::Button(btnLabel, ImVec2(btnWidth, 0)) && !lockWorking;
    ImGui::PopStyleColor(4);

    if (pressed)
    {
        lockErrMsg[0] = '\0';
        std::string pw(lockPwBuf);

        if (pw.empty())
        {
            StrToCharBuf("Please enter a password.", lockErrMsg, sizeof(lockErrMsg));
        }
        else if (isSetup)
        {
            if (pw != std::string(lockConfirmBuf))
            {
                StrToCharBuf("Passwords do not match.", lockErrMsg, sizeof(lockErrMsg));
            }
            else if (pw.size() < 6)
            {
                StrToCharBuf("Password must be at least 6 characters.", lockErrMsg, sizeof(lockErrMsg));
            }
            else
            {
                lockWorking = true;
                if (pm.SetupMasterPassword(pw))
                {
                    memset(lockPwBuf,      0, sizeof(lockPwBuf));
                    memset(lockConfirmBuf, 0, sizeof(lockConfirmBuf));
                    lockShowPw      = false;
                    lockShowConfirm = false;
                    appState        = AppState::Vault;
                    lastActivityTime = glfwGetTime();
                }
                else
                {
                    StrToCharBuf("Failed to create vault. Try again.", lockErrMsg, sizeof(lockErrMsg));
                }
                lockWorking = false;
            }
        }
        else  // Locked – verify
        {
            lockWorking = true;
            bool ok = pmInitialized
                ? pm.VerifyMasterPassword(pw)          // vault already in memory, just verify
                : pm.UnlockWithMasterPassword(pw);     // first unlock: also decrypts vault key

            if (ok)
            {
                if (!pmInitialized)
                {
                    pm.LoadFromFile();
                    pmInitialized = true;
                    std::string genPw = PasswordManager::GeneratePassword(
                        genLength, genUpper, genLower, genDigits, genSymbols);
                    StrToCharBuf(genPw, genPreview, sizeof(genPreview));
                }
                memset(lockPwBuf, 0, sizeof(lockPwBuf));
                lockShowPw       = false;
                appState         = AppState::Vault;
                lastActivityTime = glfwGetTime();
            }
            else
            {
                StrToCharBuf("Incorrect password. Please try again.", lockErrMsg, sizeof(lockErrMsg));
            }
            lockWorking = false;
        }
    }

    // Enter key submits
    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter))
        if (!lockWorking && strlen(lockPwBuf) > 0)
        {
            // Simulate button press by tagging next frame – simplest: just re-use pressed path
            // (ImGui processes key the same frame; pressing Enter here triggers the button.)
        }

    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
}

// ============================================================
//  FRAME::WindowDevelopment
// ============================================================
inline void FRAME::WindowDevelopment()
{
    // ---- First-frame: determine whether we need setup or lock ----
    if (!appStateChecked)
    {
        appState        = pm.HasMasterPassword() ? AppState::Locked : AppState::Setup;
        appStateChecked = true;
        lastActivityTime = glfwGetTime();
    }

    // ============================================================
    //  Full-screen root window  (no OS decoration – we draw our own)
    // ============================================================
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, TC(ImVec4(0.067f, 0.075f, 0.118f, 1.0f),
                                                 ImVec4(0.960f, 0.948f, 0.922f, 1.0f)));

    ImGui::Begin("##passvault", nullptr,
        ImGuiWindowFlags_NoDecoration          |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNav);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    ImVec2      winSP = ImGui::GetWindowPos();   // screen-space origin

    // ============================================================
    //  Custom Title Bar
    // ============================================================
    const float TH    = 40.0f;   // title bar height (px)
    const float SETTINGSW = 215.0f;   // settings tab width
    const float THEMEW = 105.0f;   // dark/light button width
    const float XW    = 46.0f;   // close button width
    const float LOGOW = 110.0f;  // logo / left drag-zone width

    // ---- Background + accent underline ----
    dl->AddRectFilled(
        winSP,
        ImVec2(winSP.x + (float)width, winSP.y + TH),
        TCU(IM_COL32(18, 16, 44, 255), IM_COL32(230, 222, 205, 255)));
    dl->AddLine(
        ImVec2(winSP.x,               winSP.y + TH - 1),
        ImVec2(winSP.x + (float)width, winSP.y + TH - 1),
        IM_COL32(108, 100, 220, 200), 1.5f);

    // ---- Logo text (draw-list only – zero cursor interaction) ----
    {
        const char* logo = "PassVault";
        ImVec2 tsz = fontTitle->CalcTextSizeA(26.0f, FLT_MAX, 0.0f, logo);
        dl->AddText(fontTitle, 26.0f,
            ImVec2(winSP.x + 14.0f, winSP.y + (TH - tsz.y) * 0.5f),
            IM_COL32(168, 158, 255, 255), logo);
    }
    
    // ---- File / View / Help menus (drawn AFTER the drag button;
    //      ImGui gives hit priority to the last-rendered item) ----
    float menuY = (TH - ImGui::GetFrameHeight()) * 0.5f;
    ImGui::SetCursorPos(ImVec2(LOGOW, menuY));

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.235f, 0.220f, 0.470f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.180f, 0.168f, 0.360f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, TC(ImVec4(0.886f, 0.902f, 0.941f, 1.0f),
                                             ImVec4(0.100f, 0.105f, 0.120f, 1.0f)));

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 0));

    // File menu
    if (ImGui::Button(" File ", ImVec2(65, 0)))   
        ImGui::OpenPopup("FilePopup");

    if (ImGui::BeginPopup("FilePopup"))
    {
        if (ImGui::MenuItem("New Entry", "Ctrl+N"))
        {
            ClearEditBuffers();
            editMode = true; isNewEntry = true; selectedIdx = -1;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) {
            shouldExit = true;
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button(" View ", ImVec2(65, 0)))
        ImGui::OpenPopup("ViewPopup");

    if (ImGui::BeginPopup("ViewPopup"))
    {
        if (ImGui::MenuItem("Clear Search"))     memset(searchBuf, 0, sizeof(searchBuf));
        if (ImGui::MenuItem("Show All Categories")) filterCatIdx = -1;
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    if (ImGui::Button(" Help ", ImVec2(65, 0)))
        ImGui::OpenPopup("HelpPopup");

    if (ImGui::BeginPopup("HelpPopup"))
    {
        ImGui::Text("PassVault v1.1");
        ImGui::Text("Built with OpenGL + Dear ImGui");
        ImGui::Text("Built by Trevor W");
        ImGui::Separator();
        ImGui::TextDisabled("Data: data/vault.dat");
        ImGui::TextDisabled("\t\t  data/master.auth");
        ImGui::TextDisabled("\t\t  data/vault.key");

        ImGui::EndPopup();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    // ---- Drag zone: covers full title bar minus the X button, dark/light button, and settings tab.
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::InvisibleButton("##titlebar_drag", ImVec2((float)width - SETTINGSW, TH));

    {
        static POINT dragMouseStart{};
        static int   dragWinStartX = 0, dragWinStartY = 0;

        // Capture start position on the first frame the button becomes active
        if (ImGui::IsItemActivated())
        {
            GetCursorPos(&dragMouseStart);
            glfwGetWindowPos(window, &dragWinStartX, &dragWinStartY);
        }
        // Apply drag using global cursor delta (stable regardless of window movement)
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f))
        {
            POINT cur{};
            GetCursorPos(&cur);
            glfwSetWindowPos(window,
                dragWinStartX + cur.x - dragMouseStart.x,
                dragWinStartY + cur.y - dragMouseStart.y);
        }
    }

    // ---- Toast  (centred in the title bar via draw list) ----
    if (toastTimer > 0.0f)
    {
        toastTimer -= ImGui::GetIO().DeltaTime;
        float  alpha = std::min(toastTimer, 1.0f);
        ImVec2 tsz   = fontSmall->CalcTextSizeA(13.0f, FLT_MAX, 0.0f, toastMsg);
        dl->AddText(fontSmall, 13.0f,
            ImVec2(winSP.x + ((float)width - tsz.x) * 0.5f,
                   winSP.y + (TH - tsz.y) * 0.5f),
            IM_COL32(52, 199, 120, (ImU32)(alpha * 255.0f)),
            toastMsg);
    }

    // ---- Settings toggle tab ----

    ImGui::SetCursorPos(ImVec2((float)width - SETTINGSW, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

    if (PurpleButton("Settings", ImVec2(XW * 2.0f, TH - 5.0f)))
    {
        settingsTab = true;
	}

    ImGui::PopStyleVar();

	// ---- Theme toggle button ----
    ImGui::SetCursorPos(ImVec2((float)width - THEMEW, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);

    // ---- DARK MODE ----
    if (theme == 0)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.78f, 0.95f, 0.14f));  // soft cool blue
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.74f, 0.95f, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.91f, 0.70f, 1.00f));  // warm moon glow
    }
    // ---- LIGHT MODE ----
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.96f, 0.75f, 0.20f, 0.16f));  // soft sunny gold
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.95f, 0.68f, 0.12f, 0.26f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.76f, 0.15f, 1.00f));  // bright sun
    }

    ImGui::PushFont(sunMoonFontBig);

    const char* icon = (theme == 0) ? "X" : "S";  

    if (ImGui::Button(icon, ImVec2(XW, TH)))
    {
		if (++theme > 1) theme = 0;
        
    }

    ImGui::PopFont();

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    // ---- Close (X) button ----
    ImGui::SetCursorPos(ImVec2((float)width - XW, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,     0,     0,     0   ));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.12f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.55f, 0.08f, 0.08f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,          TC(ImVec4(0.90f, 0.90f, 0.90f, 1.0f),
                                                      ImVec4(0.18f, 0.16f, 0.14f, 1.0f)));
    if (ImGui::Button("  X  ", ImVec2(XW, TH))) {
		shouldExit = true;
    }
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    // ============================================================
    //  Lock / Setup screen  (shown instead of vault content)
    // ============================================================
    if (appState != AppState::Vault)
    {
        RenderLockScreen();
        ImGui::End();
        RenderDeleteConfirmPopup();
        RenderGenPopup();
        return;
    }

    // ============================================================
	//  Settings screen (overlays vault content as a card) – triggered by Settings button in title bar
    // ============================================================
    if (settingsTab)
    {
        RenderSettingsScreen();
        ImGui::End();
        return;
	}

    // ---- Build filtered entry list (vault only) ----
    std::vector<int> filtered;
    filtered.reserve(pm.entries.size());
    {
        std::string searchLow = searchBuf;
        std::transform(searchLow.begin(), searchLow.end(), searchLow.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });

        for (int i = 0; i < (int)pm.entries.size(); ++i)
        {
            const auto& e = pm.entries[i];
            if (filterCatIdx >= 0 && e.category != CATEGORIES[filterCatIdx]) continue;
            if (!searchLow.empty())
            {
                auto toLow = [](std::string s) {
                    std::transform(s.begin(), s.end(), s.begin(),
                        [](unsigned char c) { return (char)std::tolower(c); });
                    return s;
                };
                bool match = toLow(e.title).find(searchLow)    != std::string::npos ||
                             toLow(e.website).find(searchLow)  != std::string::npos ||
                             toLow(e.username).find(searchLow) != std::string::npos ||
                             toLow(e.notes).find(searchLow)    != std::string::npos;
                if (!match) continue;
            }
            filtered.push_back(i);
        }
    }

    // ============================================================
    //  Content area  (sidebar + divider + detail)
    // ============================================================
    const float GRIP      = 18.0f;   // resize grip size
    const float SIDEBAR_W = 350.0f;
    const float contentH  = (float)height - TH;

    ImGui::SetCursorPos(ImVec2(0, TH));

    // Sidebar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, TC(ImVec4(0.047f, 0.055f, 0.086f, 1.0f),
                                                ImVec4(0.940f, 0.926f, 0.898f, 1.0f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 0));
    ImGui::BeginChild("##sidebar", ImVec2(SIDEBAR_W, contentH), false, ImGuiWindowFlags_NoScrollbar);
    RenderSidebar(filtered);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 0);

    // 1-px vertical divider
    ImGui::PushStyleColor(ImGuiCol_ChildBg, TC(ImVec4(0.118f, 0.137f, 0.212f, 1.0f),
                                                ImVec4(0.712f, 0.692f, 0.648f, 1.0f)));
    ImGui::BeginChild("##divider", ImVec2(1, contentH), false);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 0);

    // Detail panel  (leave GRIP px at the bottom for the resize handle)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, TC(ImVec4(0.078f, 0.086f, 0.133f, 1.0f),
                                                ImVec4(0.960f, 0.948f, 0.922f, 1.0f)));
    ImGui::BeginChild("##detail", ImVec2(-1, contentH - GRIP), false, ImGuiWindowFlags_NoScrollbar);
    RenderDetailPanel();
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ============================================================
    //  Resize grip  (bottom-right corner)
    // ============================================================
    ImGui::SetCursorPos(ImVec2((float)width - GRIP, (float)height - GRIP));
    ImGui::InvisibleButton("##resize_grip", ImVec2(GRIP, GRIP));

    if (ImGui::IsItemHovered())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);

    {
        static POINT resizeMouseStart{};
        static int   resizeStartW = 0, resizeStartH = 0;

        if (ImGui::IsItemActivated())
        {
            GetCursorPos(&resizeMouseStart);
            resizeStartW = width;
            resizeStartH = height;
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 1.0f))
        {
            POINT cur{};
            GetCursorPos(&cur);

            int newW = std::max(800, resizeStartW + static_cast<int>(cur.x - resizeMouseStart.x));
            int newH = std::max(500, resizeStartH + static_cast<int>(cur.y - resizeMouseStart.y));
            glfwSetWindowSize(window, newW, newH);
        }
    }

    // Visual: three diagonal lines (classic resize indicator)
    {
        ImVec2 g0  = ImVec2(winSP.x + (float)width - GRIP, winSP.y + (float)height - GRIP);
        ImU32  gc  = IM_COL32(108, 100, 220, 150);
        for (int i = 1; i <= 3; ++i)
        {
            float off = (float)(i * 5);
            dl->AddLine(
                ImVec2(g0.x + off,        g0.y + GRIP - 2),
                ImVec2(g0.x + GRIP - 2,   g0.y + off),
                gc, 1.3f);
        }
    }

    ImGui::End();

    // Modals
    RenderDeleteConfirmPopup();
    RenderGenPopup();

}

// ============================================================
//  FRAME::framebuffer_size_callback
// ============================================================
static void FRAME::framebuffer_size_callback(GLFWwindow* /*win*/, int w, int h)
{
    glViewport(0, 0, w, h);
    FRAME::width  = w;
    FRAME::height = h;
}

// ============================================================
//  FRAME::RenderLoop
// ============================================================
inline void FRAME::RenderLoop()
{
    SetupImGuiStyle();

    while (!glfwWindowShouldClose(window))
    {
        if (theme != filterTheme)
        {
            switch (theme)
            {
                case 0:
                    DarkTheme();
                    filterTheme = 0;
                    break;
                case 1:
                    LightTheme();
                    filterTheme = 1;
                    break;
                default:
                    DarkTheme();
                    filterTheme = 0;
                    break;
            }
        }

        if (shouldExit)
            glfwSetWindowShouldClose(window, true);

        glfwPollEvents();

        glClear(GL_COLOR_BUFFER_BIT);
        if (theme == 0)
            glClearColor(0.067f, 0.075f, 0.118f, 1.0f);
        else
            glClearColor(0.960f, 0.948f, 0.922f, 1.0f);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ---- Activity tracking + auto-lock ----
        {
            ImGuiIO& io = ImGui::GetIO();
            bool active = fabsf(io.MouseDelta.x) > 0.5f || fabsf(io.MouseDelta.y) > 0.5f
                       || io.MouseWheel != 0.0f
                       || io.InputQueueCharacters.Size > 0;
            if (!active)
                for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); ++i)
                    if (io.MouseDown[i]) { active = true; break; }

            if (active) lastActivityTime = glfwGetTime();

            if (appState == AppState::Vault &&
                glfwGetTime() - lastActivityTime > autoLockTimeout && autoLockTimeout != -1)
            {
                appState = AppState::Locked;
                lockErrMsg[0] = '\0';
                memset(lockPwBuf, 0, sizeof(lockPwBuf));
            }
        }

        WindowDevelopment();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
}

// ============================================================
//  FRAME::SetupFrame
// ============================================================
inline void FRAME::SetupFrame()
{
    CRYPTO::Init();

    InitGlfwFlags();
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);   // we draw our own title bar

    window = glfwCreateWindow(width, height, "PassVault - Password Manager", nullptr, nullptr);

    if (!window)
    {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
        return;
    }

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetWindowSizeLimits(window, 800, 500, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD\n";
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    glViewport(0, 0, width, height);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    RenderLoop();
}

// ============================================================
//  FRAME::UnloadFrame
// ============================================================
inline void FRAME::UnloadFrame()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}