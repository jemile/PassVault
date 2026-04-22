// PassVault
// Copyright (c) 2026 Trevor W
// SPDX-License-Identifier: MIT

// ============================================================
//  panels.h  –  Vault content panel renderers
// ============================================================

#pragma once
#include <vector>
#include "app_state.h"

// Reset all entry-edit form buffers and state
void ClearEditBuffers(UIState& s);

// Render the left-hand sidebar (search, filter, entry list)
void RenderSidebar(const std::vector<int>& filtered, UIState& s);

// Render the right-hand detail panel (welcome, edit form, or view mode)
void RenderDetailPanel(UIState& s);
