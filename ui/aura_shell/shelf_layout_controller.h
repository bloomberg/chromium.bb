// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_SHELF_LAYOUT_CONTROLLER_H_
#define UI_AURA_SHELL_SHELF_LAYOUT_CONTROLLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/gfx/compositor/layer_animation_observer.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"

namespace views {
class Widget;
}

namespace aura_shell {
namespace internal {

// ShelfLayoutController is responsible for showing and hiding the launcher and
// status area as well as positioning them.
class ShelfLayoutController : public ui::LayerAnimationObserver {
 public:
  ShelfLayoutController(views::Widget* launcher,
                        views::Widget* status);
  virtual ~ShelfLayoutController();

  // Stops any animations and sets the bounds of the launcher and status
  // widgets.
  void LayoutShelf();

  // Sets the visbility of the shelf to |visible|.
  void SetVisible(bool visible);

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

  // Current visibility. When the visibility changes this field is reset once
  // the animation completes.
  bool visible_;

  // Max height needed.
  int max_height_;

  views::Widget* launcher_;
  views::Widget* status_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutController);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_SHELF_LAYOUT_CONTROLLER_H_
