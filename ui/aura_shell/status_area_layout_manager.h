// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_STATUS_AREA_LAYOUT_MANAGER_H_
#define UI_AURA_SHELL_STATUS_AREA_LAYOUT_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/layout_manager.h"

namespace aura_shell {
namespace internal {

class ShelfLayoutManager;

// StatusAreaLayoutManager is a layout manager responsible for the status area.
// In any case when status area needs relayout it redirects this call to
// ShelfLayoutManager.
class StatusAreaLayoutManager : public aura::LayoutManager {
 public:
  explicit StatusAreaLayoutManager(ShelfLayoutManager* shelf);
  virtual ~StatusAreaLayoutManager();

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

 private:
  // Updates layout of the status area. Effectively calls ShelfLayoutManager
  // to update layout of the shelf.
  void LayoutStatusArea();

  // True when inside LayoutStatusArea method.
  // Used to prevent calling itself again from SetChildBounds().
  bool in_layout_;

  ShelfLayoutManager* shelf_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaLayoutManager);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_STATUS_AREA_LAYOUT_MANAGER_H_
