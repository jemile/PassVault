// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  app_state.h  -  Shared application & UI state
//
//  Defines the AppState enum and the UIState struct that holds
//  every piece of mutable GUI state in one place.
//  A single global instance (g_ui) is defined in frame.cpp
//  and declared extern here so every render TU can reach it.
// ============================================================

#pragma once
#include <string>
#include <vector>
#include "password_manager.h"
#include "toast.h"


// ============================================================
//  Application-Level State Enum
// ============================================================
enum class AppState { Setup, Locked, Vault };

// ============================================================
//  UIState  -  All mutable GUI state in one struct
//
//  Groupings:
//    App-level    - which screen is active, first-frame flag
//    Vault        - PasswordManager instance + init flag
//    Lock screen  - field visibility, error, working flag,
//                   activity timer, input buffers
//    Edit form    - mode flags, entry ID, all field buffers,
//                   tag chips + new-tag input + palette pick
//    Browsing     - selected index, popup flags
//    Filter       - active tag string, favorites toggle, search
//    Generator    - settings + preview buffer
//    Toast        - message string + fade timer
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
    std::string editingId;
    char        editTitle[128]    = {};
    char        editWebsite[256]  = {};
    char        editUsername[256] = {};
    char        editPassword[256] = {};
    char        editNotes[2048]   = {};

    // Tag editing (replaces old editCatIdx + CATEGORIES combo)
    std::vector<std::string> editTags;   // tags for the currently-edited entry
    char        newTagBuf[64]   = {};    // text field for typing a new tag name
    int         newTagColorIdx  = 0;     // selected palette index (0-9) for new tag

    // ---- Vault browsing ----
	int         selectedIdx      = -1; // currently selected entry in the sidebar list
    int         pendingDeleteIdx =  -1; // right click delete
    bool        showDeleteConfirm = false;
    bool        showPassword      = false;
    bool        showGenPopup      = false;
    bool        showHistoryPopup  = false;  // password-history modal

    // ---- Sidebar / search / filter ----
    std::string filterTag;                    // active tag filter (empty = "All")
    std::string filterFolder;                 // active folder filter (empty = no folder filter)
    bool        filterFavorites      = false; // when true only show starred entries
    char        searchBuf[256]       = {};
    std::string pendingDeleteTag;
    bool        showDeleteTagConfirm = false;

    // ---- Password generator ----
    int         genLength  = 16;
    bool        genUpper   = true;
    bool        genLower   = true;
    bool        genDigits  = true;
    bool        genSymbols = true;
    char        genPreview[256] = {};

    // ---- Toast notifications (bottom-right stack) ----
    std::vector<ToastEntry> toasts;
    bool  toastsEnabled  = true;
    float toastDuration  = 4.0f;   // seconds each toast is shown (2–8)

    // ---- Entry edit form - folder ----
    char  editFolderBuf[64]  = {};

    // ---- Create-folder inline input (sidebar footer) ----
    bool  showNewFolderInput = false;
    char  newFolderBuf[64]   = {};

    // ---- Change master password (settings screen) ----
    char  changePwCurrent[256]  = {};
    char  changePwNew[256]      = {};
    char  changePwConfirm[256]  = {};
    bool  changePwShowCurrent   = false;
    bool  changePwShowNew       = false;
    bool  changePwShowConfirm   = false;
};


// Single global instance - defined in frame.cpp
extern UIState g_ui;