// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_AURA_SWITCHES_H_
#define UI_AURA_AURA_SWITCHES_H_
#pragma once

#include "ui/aura/aura_export.h"

namespace switches {

// Please keep alphabetized.
AURA_EXPORT extern const char kAuraHostWindowSize[];
AURA_EXPORT extern const char kAuraNoShadows[];
AURA_EXPORT extern const char kAuraTranslucentFrames[];
AURA_EXPORT extern const char kAuraWindowMode[];
AURA_EXPORT extern const char kAuraWindowModeCompact[];
AURA_EXPORT extern const char kAuraWindowModeNormal[];
AURA_EXPORT extern const char kAuraWorkspaceManager[];

// Utilities for testing multi-valued switches.
AURA_EXPORT bool IsAuraWindowModeCompact();

}  // namespace switches

#endif  // UI_AURA_AURA_SWITCHES_H_
