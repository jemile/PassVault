// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  popups.h  -  Modal popup renderers
// ============================================================

#pragma once
#include "app_state.h"

// Password generator modal
void RenderGenPopup(UIState& s);

// Entry deletion confirmation modal for entries
void RenderDeleteConfirmPopup(UIState& s);
 
// Entry deletion confirmation modal for tags
void RenderDeleteTagConfirmPopup(UIState& s);

// Password history modal (shows previous versions of an entry's password)
void RenderHistoryPopup(UIState& s);
