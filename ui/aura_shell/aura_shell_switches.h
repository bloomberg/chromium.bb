// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_AURA_SHELL_SWITCHES_H_
#define UI_AURA_SHELL_AURA_SHELL_SWITCHES_H_
#pragma once

#include "ui/aura_shell/aura_shell_export.h"

namespace aura_shell {
namespace switches {

// Please keep alphabetized.
AURA_SHELL_EXPORT extern const char kAuraNoShadows[];
AURA_SHELL_EXPORT extern const char kAuraTranslucentFrames[];
AURA_SHELL_EXPORT extern const char kAuraViewsAppList[];
AURA_SHELL_EXPORT extern const char kAuraWindowMode[];
AURA_SHELL_EXPORT extern const char kAuraWindowModeCompact[];
AURA_SHELL_EXPORT extern const char kAuraWindowModeNormal[];
AURA_SHELL_EXPORT extern const char kAuraWorkspaceManager[];

// Utilities for testing multi-valued switches.
AURA_SHELL_EXPORT bool IsAuraWindowModeCompact();

}  // namespace switches
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_AURA_SHELL_SWITCHES_H_
