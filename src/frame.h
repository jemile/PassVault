// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

#pragma once
#include <thread>

#include "glad.h"
#include <GLFW/glfw3.h>
#include <ImGui/imgui.h>
#include <ImGui/imgui_impl_glfw.h>
#include <ImGui/imgui_impl_opengl3.h>

namespace FRAME
{
    extern int         width;
    extern int         height;
    extern int         menuTab;
    extern bool        shouldExit;

    extern GLFWwindow* window;

    extern ImFont*     font;        // regular  (~16 px)
    extern ImFont*     fontTitle;   // headings (~21 px)
    extern ImFont*     fontSmall;   // captions (~13 px)

    extern void framebuffer_size_callback(GLFWwindow* window, int w, int h);

    extern void SetupImGuiStyle();
    extern void InitGlfwFlags();
    extern void CenterSpacing(const char* label);
    extern void RenderLoop();
    extern void WindowDevelopment();
    extern void SetupFrame();
    extern void UnloadFrame();

}
