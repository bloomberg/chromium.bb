// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_COMPACT_LAYOUT_MANAGER_H_
#define UI_AURA_SHELL_COMPACT_LAYOUT_MANAGER_H_
#pragma once

#include <set>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window_observer.h"
#include "ui/aura_shell/aura_shell_export.h"

namespace views {
class Widget;
}

namespace aura_shell {
namespace internal {

// CompactLayoutManager is an alternate LayoutManager for the container that
// hosts what the shell considers to be top-level windows. It is used for low
// resolution screens and keeps the main browser window maximized.
// It listens for changes to kShowStateKey and resizes the window appropriately.
class AURA_SHELL_EXPORT CompactLayoutManager : public aura::LayoutManager,
                                               public aura::WindowObserver {
 public:
  explicit CompactLayoutManager(views::Widget* status_area_widget);
  virtual ~CompactLayoutManager();

  // LayoutManager overrides:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  // WindowObserver overrides:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const char* name,
                                       void* old) OVERRIDE;

 private:
  typedef std::set<aura::Window*> Windows;

  // Hides the status area when full screen windows cover it.
  void UpdateStatusAreaVisibility();

  // Set of windows we're listening to.
  Windows windows_;

  // Weak pointer to status area with clock, network, battery, etc. icons.
  views::Widget* status_area_widget_;

  DISALLOW_COPY_AND_ASSIGN(CompactLayoutManager);
};

}  // namespace aura_shell
}  // namespace internal

#endif  // UI_AURA_SHELL_COMPACT_LAYOUT_MANAGER_H_
