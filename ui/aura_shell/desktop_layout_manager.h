// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_DESKTOP_LAYOUT_MANAGER_H_
#define UI_AURA_SHELL_DESKTOP_LAYOUT_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/layout_manager.h"

namespace aura {
class Window;
}
namespace gfx {
class Rect;
}
namespace views {
class Widget;
}

namespace aura_shell {
namespace internal {

class ShelfLayoutController;

// A layout manager for the root window.
// Resizes all of its immediate children to fill the bounds of the root window.
class DesktopLayoutManager : public aura::LayoutManager {
 public:
  explicit DesktopLayoutManager(aura::Window* owner);
  virtual ~DesktopLayoutManager();

  void set_shelf(ShelfLayoutController* shelf) { shelf_ = shelf; }

  void set_background_widget(views::Widget* background_widget) {
    background_widget_ = background_widget;
  }

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

 private:
  aura::Window* owner_;

  views::Widget* background_widget_;

  ShelfLayoutController* shelf_;

  DISALLOW_COPY_AND_ASSIGN(DesktopLayoutManager);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_DESKTOP_LAYOUT_MANAGER_H_
