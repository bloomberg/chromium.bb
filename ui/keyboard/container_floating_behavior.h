// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_CONTAINER_FLOATING_BEHAVIOR_H_
#define UI_KEYBOARD_CONTAINER_FLOATING_BEHAVIOR_H_

#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/keyboard/container_behavior.h"
#include "ui/keyboard/keyboard_export.h"
#include "ui/wm/core/window_animations.h"

namespace keyboard {

class KEYBOARD_EXPORT ContainerFloatingBehavior : public ContainerBehavior {
 public:
  ~ContainerFloatingBehavior() override;

  // ContainerBehavior overrides
  void DoHidingAnimation(
      aura::Window* window,
      ::wm::ScopedHidingAnimationSettings* animation_settings) override;
  void DoShowingAnimation(
      aura::Window* window,
      ui::ScopedLayerAnimationSettings* animation_settings) override;
  void InitializeShowAnimationStartingState(aura::Window* container) override;
  const gfx::Rect AdjustSetBoundsRequest(
      const gfx::Rect& display_bounds,
      const gfx::Rect& requested_bounds) const override;
  bool IsOverscrollAllowed() const override;
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_CONTAINER_FLOATING_BEHAVIOR_H_
