// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHELF_LAYOUT_MANAGER_H_
#define UI_AURA_SHELL_SHELF_LAYOUT_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/layout_manager.h"
#include "ui/gfx/compositor/layer_animation_observer.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"

namespace views {
class Widget;
}

namespace aura_shell {
namespace internal {

// ShelfLayoutManager is a layout manager responsible for the launcher.
// Also supports showing and hiding the launcher/status area
// as well as positioning them.
class ShelfLayoutManager : public aura::LayoutManager,
                           public ui::LayerAnimationObserver {
 public:
  ShelfLayoutManager(views::Widget* launcher,
                     views::Widget* status);
  virtual ~ShelfLayoutManager();

  bool in_layout() const { return in_layout_; }

  // Stops any animations and sets the bounds of the launcher and status
  // widgets.
  void LayoutShelf();

  // Sets the visbility of the shelf to |visible|.
  void SetVisible(bool visible);

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

 private:
  struct TargetBounds {
    gfx::Rect launcher_bounds;
    gfx::Rect status_bounds;
    gfx::Insets work_area_insets;
  };

  // Stops any animations.
  void StopAnimating();

  // Calculates the target bounds assuming visibility of |visibile|.
  void CalculateTargetBounds(bool visible,
                             TargetBounds* target_bounds);

  // Animates |widget| to the specified bounds and opacity.
  void AnimateWidgetTo(views::Widget* widget,
                       const gfx::Rect& target_bounds,
                       float target_opacity);

  // LayerAnimationObserver overrides:
  virtual void OnLayerAnimationEnded(
      const ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      const ui::LayerAnimationSequence* sequence) OVERRIDE {}
  virtual void OnLayerAnimationScheduled(
      const ui::LayerAnimationSequence* sequence) OVERRIDE {}

  // Are we animating?
  bool animating_;

  // True when inside LayoutShelf method. Used to prevent calling LayoutShelf
  // again from SetChildBounds().
  bool in_layout_;

  // Current visibility. When the visibility changes this field is reset once
  // the animation completes.
  bool visible_;

  // Max height needed.
  int max_height_;

  views::Widget* launcher_;
  views::Widget* status_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManager);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_SHELF_LAYOUT_MANAGER_H_
