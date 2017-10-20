// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_CONTAINER_BEHAVIOR_H_
#define UI_KEYBOARD_CONTAINER_BEHAVIOR_H_

#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/keyboard/keyboard_export.h"
#include "ui/wm/core/window_animations.h"

namespace keyboard {

// Represents and encapsulates how the keyboard container should visually behave
// within the workspace window.
class KEYBOARD_EXPORT ContainerBehavior {
 public:
  virtual ~ContainerBehavior() {}

  // Apply changes to the animation settings to animate the keyboard container
  // showing.
  virtual void DoShowingAnimation(
      aura::Window* window,
      ui::ScopedLayerAnimationSettings* animation_settings) = 0;

  // Apply changes to the animation settings to animate the keyboard container
  // hiding.
  virtual void DoHidingAnimation(
      aura::Window* window,
      ::wm::ScopedHidingAnimationSettings* animation_settings) = 0;

  // Initialize the starting state of the keyboard container for the showing
  // animation.
  virtual void InitializeShowAnimationStartingState(
      aura::Window* container) = 0;

  virtual const gfx::Rect AdjustSetBoundsRequest(
      const gfx::Rect& display_bounds,
      const gfx::Rect& requested_bounds) const = 0;

  // A ContainerBehavior can choose to not allow overscroll to be used. It is
  // important to note that the word "Allowed" is used because whether or not
  // overscroll is "enabled" depends on multiple external factors.
  virtual bool IsOverscrollAllowed() const = 0;
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_CONTAINER_BEHAVIOR_H_
