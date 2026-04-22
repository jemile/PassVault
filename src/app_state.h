// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  app_state.h  –  Shared application & UI state
//
//  Defines the AppState enum, entry category constants, and
//  the UIState struct that holds every piece of mutable GUI
//  state in one place.  A single global instance (g_ui) is
//  defined in frame.cpp and declared extern here so every
//  render translation unit can reach it.
// ============================================================

#pragma once
#include <string>
#include "password_manager.h"


// ============================================================
//  Application-Level State Enum
// ============================================================
enum class AppState { Setup, Locked, Vault };


// ============================================================
//  Entry Categories  (shared constants)
// ============================================================
static const char* CATEGORIES[] = { "Personal", "Work", "Finance", "Social", "Other" };
static const int   NUM_CATS     = 5;


// ============================================================
//  UIState  –  All mutable GUI state in one struct
//
//  Groupings mirror the logical sections of the application:
//    App-level   - which screen is active, first-frame flag
//    Vault       - the PasswordManager instance + init flag
//    Lock screen - field visibility, error text, working flag,
//                  activity timer, input buffers
//    Edit form   - mode flags, category index, entry ID,
//                  all text-field buffers
//    Browsing    - selected entry index, confirm/show flags
//    Filter      - active category index, search buffer
//    Generator   - settings + preview buffer
//    Toast       - message string + fade timer
// ============================================================
struct UIState
{
    // ---- App-level ----
    AppState    appState        = AppState::Locked;
    bool        appStateChecked = false;

    // ---- Vault core ----
    PasswordManager pm;
    bool            pmInitialized = false;

    // ---- Lock screen ----
    bool        lockShowPw       = false;
    bool        lockShowConfirm  = false;
    char        lockErrMsg[128]  = {};
    bool        lockWorking      = false;
    double      lastActivityTime = 0.0;
    char        lockPwBuf[256]      = {};
    char        lockConfirmBuf[256] = {};

    // ---- Entry edit form ----
    bool        editMode    = false;
    bool        isNewEntry  = false;
    int         editCatIdx  = 0;
    std::string editingId   = "";
    char        editTitle[128]    = {};
    char        editWebsite[256]  = {};
    char        editUsername[256] = {};
    char        editPassword[256] = {};
    char        editNotes[2048]   = {};

    // ---- Vault browsing ----
    int         selectedIdx       = -1;
    bool        showDeleteConfirm = false;
    bool        showPassword      = false;
    bool        showGenPopup      = false;

    // ---- Sidebar / search / filter ----
    int         filterCatIdx  = -1;     // -1 = All
    char        searchBuf[256] = {};

    // ---- Password generator ----
    int         genLength  = 16;
    bool        genUpper   = true;
    bool        genLower   = true;
    bool        genDigits  = true;
    bool        genSymbols = true;
    char        genPreview[256] = {};

    // ---- Toast notification ----
    char        toastMsg[128] = {};
    float       toastTimer    = 0.0f;
};


// Single global instance – defined in frame.cpp
extern UIState g_ui;
