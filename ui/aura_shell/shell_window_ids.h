// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHELL_WINDOW_IDS_H_
#define UI_AURA_SHELL_SHELL_WINDOW_IDS_H_
#pragma once

// Declarations of ids of special shell windows.

namespace aura_shell {

namespace internal {

// The desktop background window.
const int kShellWindowId_DesktopBackgroundContainer = 0;

// The container for standard top-level windows.
const int kShellWindowId_DefaultContainer = 1;

// The container for top-level windows with the 'always-on-top' flag set.
const int kShellWindowId_AlwaysOnTopContainer = 2;

// The container for the launcher.
const int kShellWindowId_LauncherContainer = 3;

// The container for user-specific modal windows.
const int kShellWindowId_ModalContainer = 4;

// The container for the lock screen.
const int kShellWindowId_LockScreenContainer = 5;

// The container for the status area.
const int kShellWindowId_StatusContainer = 6;

// The container for menus and tooltips.
const int kShellWindowId_MenusAndTooltipsContainer = 7;

}  // namespace internal

}  // namespace aura_shell


#endif  // UI_AURA_SHELL_SHELL_WINDOW_IDS_H_
