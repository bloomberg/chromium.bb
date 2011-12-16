// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHADOW_TYPES_H_
#define UI_AURA_SHELL_SHADOW_TYPES_H_
#pragma once

#include "ui/aura_shell/aura_shell_export.h"

namespace aura {
class Window;
}

namespace aura_shell {
namespace internal {

// Different types of drop shadows that can be drawn under a window by the
// shell.  Used as a value for the kShadowTypeKey property.
enum ShadowType {
  // Starts at 0 due to the cast in GetShadowType().
  SHADOW_TYPE_NONE = 0,
  SHADOW_TYPE_RECTANGULAR,
};

AURA_SHELL_EXPORT void SetShadowType(aura::Window* window,
                                     ShadowType shadow_type);
AURA_SHELL_EXPORT ShadowType GetShadowType(aura::Window* window);

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_SHADOW_TYPES_H_
