// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WS_AX_ASH_WINDOW_UTILS_H_
#define ASH_WS_AX_ASH_WINDOW_UTILS_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/accessibility/ax_aura_window_utils.h"

namespace ash {

// Provides functions for walking a tree of aura::Windows for accessibility. For
// SingleProcessMash we want the accessibility tree to jump from a proxy aura
// Window on the ash side directly to its corresponding client window. This is
// just a temporary solution to that issue and should be removed once Mash is
// fully launched. https://crbug.com/911945
class ASH_EXPORT AXAshWindowUtils : public views::AXAuraWindowUtils {
 public:
  AXAshWindowUtils();
  ~AXAshWindowUtils() override;

  // views::AXAuraWindowUtils:
  aura::Window* GetParent(aura::Window* window) override;
  aura::Window::Windows GetChildren(aura::Window* window) override;
  bool IsRootWindow(aura::Window* window) const override;
  views::Widget* GetWidgetForNativeView(aura::Window* window) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AXAshWindowUtils);
};

}  // namespace ash

#endif  // ASH_WS_AX_ASH_WINDOW_UTILS_H_
