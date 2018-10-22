// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_WM_CORE_SHADOW_CONTROLLER_DELEGATE_H_
#define UI_WM_CORE_SHADOW_CONTROLLER_DELEGATE_H_

#include "ui/wm/core/wm_core_export.h"

namespace aura {
class Window;
}

namespace wm {

// ShadowControllerDelegate allows a user to modify a shadow on certain windows
// differently from the normal use case.
class WM_CORE_EXPORT ShadowControllerDelegate {
 public:
  ShadowControllerDelegate() = default;
  virtual ~ShadowControllerDelegate() = default;

  // Invoked when the shadow on |window| is to be modified, either normally from
  // activation change or manually.
  virtual bool ShouldShowShadowForWindow(const aura::Window* window) = 0;
};

}  // namespace wm

#endif  // UI_WM_CORE_SHADOW_CONTROLLER_DELEGATE_H_