// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  frame.cpp  –  Window lifecycle, render loop, and orchestration
//
//  Owns: FRAME globals, UIState g_ui, title bar, resize grip,
//        layout helpers, and the main window / render loop.
//  Delegates all panel/screen/popup drawing to their own TUs.
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
#include "panels.h"
#include "screens.h"
#include "popups.h"
#include "render.h"
#include "crypto.h"
#include "hatten_font.h"
#include "sun_moon_font.h"


// ============================================================
//  FRAME Namespace – Global Variables
// ============================================================
namespace FRAME
{
    // Window & Display State
    int         width       = 1280;
    int         height      = 720;
    int         theme       = 0;
    int         filterTheme = -1;   // -1 forces theme apply on first frame
    bool        shouldExit  = false;
    bool        settingsTab = false;

    // Auto-Lock Settings
    int         autoLockIndex   = 1;
    float       autoLockTimeout = 60.0f;

    // OpenGL / ImGui Handles
    GLFWwindow* window      = nullptr;
    ImFont*     font        = nullptr;
    ImFont*     fontTitle   = nullptr;
    ImFont*     fontSmall   = nullptr;
    ImFont*     sunMoonFontBig = nullptr;
}


// ============================================================
//  Global UIState Instance
// ============================================================
UIState g_ui;


// ============================================================
//  State: Determine initial app state on the first frame
// ============================================================
static void CheckInitialAppState()
{
    if (g_ui.appStateChecked) return;

    g_ui.appState        = g_ui.pm.HasMasterPassword() ? AppState::Locked : AppState::Setup;
    g_ui.appStateChecked = true;
    g_ui.lastActivityTime = glfwGetTime();
}


// ============================================================
//  State: Apply theme colors whenever the theme selection changes
// ============================================================
static void ApplyThemeIfChanged()
{
    using namespace FRAME;

    if (theme == filterTheme) return;

    switch (theme)
    {
        case 1:  THEME::LightTheme(); filterTheme = 1; break;
        default: THEME::DarkTheme();  filterTheme = 0; break;
    }
}


// ============================================================
//  State: Track user activity and trigger auto-lock on timeout
// ============================================================
static void UpdateActivityTracking()
{
    using namespace FRAME;

    ImGuiIO& io     = ImGui::GetIO();
    bool     active = fabsf(io.MouseDelta.x) > 0.5f
                   || fabsf(io.MouseDelta.y) > 0.5f
                   || io.MouseWheel != 0.0f
                   || io.InputQueueCharacters.Size > 0;

    if (!active)
        for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); ++i)
            if (io.MouseDown[i]) { active = true; break; }

    if (active) g_ui.lastActivityTime = glfwGetTime();

    if (g_ui.appState == AppState::Vault &&
        glfwGetTime() - g_ui.lastActivityTime > autoLockTimeout &&
        autoLockTimeout != -1)
    {
        g_ui.appState      = AppState::Locked;
        g_ui.lockErrMsg[0] = '\0';
        memset(g_ui.lockPwBuf, 0, sizeof(g_ui.lockPwBuf));
    }
}


// ============================================================
//  Data: Build the filtered + searched entry index list
// ============================================================
static std::vector<int> BuildFilteredList()
{
    using namespace FRAME;

    std::vector<int> filtered;
    filtered.reserve(g_ui.pm.entries.size());

    std::string searchLow = g_ui.searchBuf;
    std::transform(searchLow.begin(), searchLow.end(), searchLow.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });

    for (int i = 0; i < (int)g_ui.pm.entries.size(); ++i)
    {
        const auto& e = g_ui.pm.entries[i];

        if (g_ui.filterCatIdx >= 0 && e.category != CATEGORIES[g_ui.filterCatIdx]) continue;

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

    return filtered;
}


// ============================================================
//  Style Setup: Load fonts and configure ImGui layout style
// ============================================================
static void SetupImGuiStyle()
{
    using namespace FRAME;

    ImGuiIO& io    = ImGui::GetIO();
    io.IniFilename = nullptr;

    // Tell ImGui to keep our font data in memory (we manage it ourselves)
    ImFontConfig cfg = {};
    cfg.FontDataOwnedByAtlas = false;

    // Load font sizes from the embedded Hatten TTF
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


// ============================================================
//  Init: Set GLFW window hints for OpenGL 3.3
// ============================================================
static void InitGlfwFlags()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);  // we draw our own title bar

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
}


// ============================================================
//  Title Bar: Background, logo, menus, drag zone, toast,
//             settings button, theme toggle, close button
// ============================================================
static void RenderTitleBar()
{
    using namespace FRAME;

    ImDrawList* dl    = ImGui::GetWindowDrawList();
    ImVec2      winSP = ImGui::GetWindowPos();

    const float TH        = 40.0f;   // title bar height (px)
    const float SETTINGSW = 215.0f;  // settings + theme + close total width
    const float THEMEW    = 105.0f;  // dark/light button width
    const float XW        = 46.0f;   // close button width
    const float LOGOW     = 110.0f;  // logo / left drag-zone width

    // Background + accent underline
    dl->AddRectFilled(
        winSP,
        ImVec2(winSP.x + (float)width, winSP.y + TH),
        THEME::TCU(IM_COL32(18, 16, 44, 255), IM_COL32(230, 222, 205, 255), theme));
    dl->AddLine(
        ImVec2(winSP.x,                winSP.y + TH - 1),
        ImVec2(winSP.x + (float)width, winSP.y + TH - 1),
        IM_COL32(108, 100, 220, 200), 1.5f);

    // Logo (draw-list only – no cursor interaction)
    {
        const char* logo = "PassVault";
        ImVec2 tsz = fontTitle->CalcTextSizeA(26.0f, FLT_MAX, 0.0f, logo);
        dl->AddText(fontTitle, 26.0f,
            ImVec2(winSP.x + 14.0f, winSP.y + (TH - tsz.y) * 0.5f),
            IM_COL32(168, 158, 255, 255), logo);
    }

    // File / View / Help menus
    float menuY = (TH - ImGui::GetFrameHeight()) * 0.5f;
    ImGui::SetCursorPos(ImVec2(LOGOW, menuY));

    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.235f, 0.220f, 0.470f, 0.7f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.180f, 0.168f, 0.360f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, THEME::TC(
        ImVec4(0.886f, 0.902f, 0.941f, 1.0f),
        ImVec4(0.100f, 0.105f, 0.120f, 1.0f), theme));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 0));

    // File menu
    if (ImGui::Button(" File ", ImVec2(65, 0)))
        ImGui::OpenPopup("FilePopup");
    if (ImGui::BeginPopup("FilePopup"))
    {
        if (ImGui::MenuItem("New Entry", "Ctrl+N"))
        {
            ClearEditBuffers(g_ui);
            g_ui.editMode   = true;
            g_ui.isNewEntry = true;
            g_ui.selectedIdx = -1;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exit"))
            shouldExit = true;
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // View menu
    if (ImGui::Button(" View ", ImVec2(65, 0)))
        ImGui::OpenPopup("ViewPopup");
    if (ImGui::BeginPopup("ViewPopup"))
    {
        if (ImGui::MenuItem("Clear Search"))        memset(g_ui.searchBuf, 0, sizeof(g_ui.searchBuf));
        if (ImGui::MenuItem("Show All Categories")) g_ui.filterCatIdx = -1;
        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // Help menu
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

    // Drag zone (covers title bar minus right-side buttons)
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImGui::InvisibleButton("##titlebar_drag", ImVec2((float)width - SETTINGSW, TH));
    {
        static POINT dragMouseStart{};
        static int   dragWinStartX = 0, dragWinStartY = 0;

        if (ImGui::IsItemActivated())
        {
            GetCursorPos(&dragMouseStart);
            glfwGetWindowPos(window, &dragWinStartX, &dragWinStartY);
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 3.0f))
        {
            POINT cur{};
            GetCursorPos(&cur);
            glfwSetWindowPos(window,
                dragWinStartX + cur.x - dragMouseStart.x,
                dragWinStartY + cur.y - dragMouseStart.y);
        }
    }

    // Toast notification (centered in title bar)
    if (g_ui.toastTimer > 0.0f)
    {
        g_ui.toastTimer -= ImGui::GetIO().DeltaTime;
        float  alpha = std::min(g_ui.toastTimer, 1.0f);
        ImVec2 tsz   = fontSmall->CalcTextSizeA(13.0f, FLT_MAX, 0.0f, g_ui.toastMsg);
        dl->AddText(fontSmall, 13.0f,
            ImVec2(winSP.x + ((float)width - tsz.x) * 0.5f,
                   winSP.y + (TH - tsz.y) * 0.5f),
            IM_COL32(52, 199, 120, (ImU32)(alpha * 255.0f)),
            g_ui.toastMsg);
    }

    // Settings toggle button
    ImGui::SetCursorPos(ImVec2((float)width - SETTINGSW, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    if (RENDER::PurpleButton("Settings", ImVec2(XW * 2.0f, TH - 5.0f)))
        settingsTab = true;
    ImGui::PopStyleVar();

    // Theme toggle button (moon in dark mode, sun in light mode)
    ImGui::SetCursorPos(ImVec2((float)width - THEMEW, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);

    if (theme == 0)  // Dark mode – show moon
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f, 0.78f, 0.95f, 0.14f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f, 0.74f, 0.95f, 0.25f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.94f, 0.91f, 0.70f, 1.00f));  // warm moon glow
    }
    else             // Light mode – show sun
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.00f, 0.00f, 0.00f, 0.00f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.96f, 0.75f, 0.20f, 0.16f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.95f, 0.68f, 0.12f, 0.26f));
        ImGui::PushStyleColor(ImGuiCol_Text,          ImVec4(0.98f, 0.76f, 0.15f, 1.00f));  // bright sun
    }

    ImGui::PushFont(sunMoonFontBig);
    // X is moon | S is sun
    const char* icon = (theme == 0) ? "X" : "S";
    if (ImGui::Button(icon, ImVec2(XW, TH)))
        if (++theme > 1) theme = 0;
    ImGui::PopFont();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    // Close (X) button
    ImGui::SetCursorPos(ImVec2((float)width - XW, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0,     0,     0,     0   ));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.80f, 0.12f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.55f, 0.08f, 0.08f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, THEME::TC(
        ImVec4(0.90f, 0.90f, 0.90f, 1.0f),
        ImVec4(0.18f, 0.16f, 0.14f, 1.0f), theme));
    if (ImGui::Button("  X  ", ImVec2(XW, TH)))
        shouldExit = true;
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
}


// ============================================================
//  Layout: Resize grip – bottom-right corner drag handle
// ============================================================
static void RenderResizeGrip()
{
    using namespace FRAME;

    const float GRIP  = 18.0f;
    ImDrawList* dl    = ImGui::GetWindowDrawList();
    ImVec2      winSP = ImGui::GetWindowPos();

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

    // Visual: Resize indicator
    {
        ImVec2 g0 = ImVec2(winSP.x + (float)width - GRIP, winSP.y + (float)height - GRIP);
        ImU32  gc = IM_COL32(108, 100, 220, 150);
        for (int i = 1; i <= 3; ++i)
        {
            float off = (float)(i * 5);
            dl->AddLine(
                ImVec2(g0.x + off,      g0.y + GRIP - 2),
                ImVec2(g0.x + GRIP - 2, g0.y + off),
                gc, 1.3f);
        }
    }
}


// ============================================================
//  Layout: Vault content area – sidebar, divider, detail panel,
//          and resize grip
// ============================================================
static void RenderContentArea(const std::vector<int>& filtered)
{
    using namespace FRAME;

    const float TH        = 40.0f;
    const float GRIP      = 18.0f;
    const float SIDEBAR_W = 350.0f;
    const float contentH  = (float)height - TH;

    ImGui::SetCursorPos(ImVec2(0, TH));

    // Sidebar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, THEME::TC(
        ImVec4(0.047f, 0.055f, 0.086f, 1.0f),
        ImVec4(0.940f, 0.926f, 0.898f, 1.0f), theme));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 0));
    ImGui::BeginChild("##sidebar", ImVec2(SIDEBAR_W, contentH), false, ImGuiWindowFlags_NoScrollbar);
    RenderSidebar(filtered, g_ui);
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 0);

    // 1-px vertical divider
    ImGui::PushStyleColor(ImGuiCol_ChildBg, THEME::TC(
        ImVec4(0.118f, 0.137f, 0.212f, 1.0f),
        ImVec4(0.712f, 0.692f, 0.648f, 1.0f), theme));
    ImGui::BeginChild("##divider", ImVec2(1, contentH), false);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0, 0);

    // Detail panel (GRIP px at the bottom for the resize handle)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, THEME::TC(
        ImVec4(0.078f, 0.086f, 0.133f, 1.0f),
        ImVec4(0.960f, 0.948f, 0.922f, 1.0f), theme));
    ImGui::BeginChild("##detail", ImVec2(-1, contentH - GRIP), false, ImGuiWindowFlags_NoScrollbar);
    RenderDetailPanel(g_ui);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    RenderResizeGrip();
}


// ============================================================
//  Window: Build and present the full application window
// ============================================================
static void WindowDevelopment()
{
    using namespace FRAME;

    CheckInitialAppState();

    // Full-screen root window (no OS decoration – we draw our own)
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, THEME::TC(
        ImVec4(0.067f, 0.075f, 0.118f, 1.0f),
        ImVec4(0.960f, 0.948f, 0.922f, 1.0f), theme));

    ImGui::Begin("##passvault", nullptr,
        ImGuiWindowFlags_NoDecoration          |
        ImGuiWindowFlags_NoMove                |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNav);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();

    RenderTitleBar();

    // Lock | Setup screen (shown instead of vault content)
    if (g_ui.appState != AppState::Vault)
    {
        RenderLockScreen(g_ui);
        ImGui::End();
        RenderDeleteConfirmPopup(g_ui);
        RenderGenPopup(g_ui);
        return;
    }

    // Settings screen (overlays vault content as a centered card)
    if (settingsTab)
    {
        RenderSettingsScreen(g_ui);
        ImGui::End();
        return;
    }

    // Build filtered entry list and render vault layout
    const std::vector<int> filtered = BuildFilteredList();
    RenderContentArea(filtered);

    ImGui::End();

    // Modals
    RenderDeleteConfirmPopup(g_ui);
    RenderGenPopup(g_ui);
}


// ============================================================
//  Callback: Update viewport and stored dimensions on resize
// ============================================================
static void framebuffer_size_callback(GLFWwindow* /*win*/, int w, int h)
{
    glViewport(0, 0, w, h);
    FRAME::width  = w;
    FRAME::height = h;
}


// ============================================================
//  Loop: Main render loop – runs until window close is requested
// ============================================================
static void RenderLoop()
{
    SetupImGuiStyle();

    while (!glfwWindowShouldClose(FRAME::window))
    {
        ApplyThemeIfChanged();

        if (FRAME::shouldExit)
            glfwSetWindowShouldClose(FRAME::window, true);

        glfwPollEvents();

        glClear(GL_COLOR_BUFFER_BIT);
        if (FRAME::theme == 0)
            glClearColor(0.067f, 0.075f, 0.118f, 1.0f);
        else
            glClearColor(0.960f, 0.948f, 0.922f, 1.0f);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        UpdateActivityTracking();
        WindowDevelopment();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(FRAME::window);
    }
}


// ============================================================
//  FRAME::SetupFrame – Initialize window, OpenGL, ImGui, and run
// ============================================================
void FRAME::SetupFrame()
{
    CRYPTO::Init();

    InitGlfwFlags();

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
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    glViewport(0, 0, width, height);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    RenderLoop();
}


// ============================================================
//  FRAME::UnloadFrame – Shut down ImGui and GLFW cleanly
// ============================================================
void FRAME::UnloadFrame()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
}
