// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_ARC_ARC_VIRTUAL_KEYBOARD_CONTAINER_LAYOUT_MANAGER_H_
#define ASH_KEYBOARD_ARC_ARC_VIRTUAL_KEYBOARD_CONTAINER_LAYOUT_MANAGER_H_

#include "base/macros.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class ArcVirtualKeyboardContainerLayoutManager : public aura::LayoutManager {
 public:
  explicit ArcVirtualKeyboardContainerLayoutManager(
      aura::Window* arc_ime_window_parent_container);

  // aura::LayoutManager:
  void OnWindowResized() override;
  void OnWindowAddedToLayout(aura::Window* child) override;
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

 private:
  aura::Window* arc_ime_window_parent_container_;

  DISALLOW_COPY_AND_ASSIGN(ArcVirtualKeyboardContainerLayoutManager);
};

}  // namespace ash

#endif  // ASH_KEYBOARD_ARC_ARC_VIRTUAL_KEYBOARD_CONTAINER_LAYOUT_MANAGER_H_
