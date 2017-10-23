// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/container_floating_behavior.h"

#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_ui.h"

namespace keyboard {

constexpr int kKeyboardWidth = 600;

constexpr int kAnimationDurationMs = 200;
constexpr float kAnimationStartOrAfterHideOpacity = 0.01f;
constexpr int kAnimationDistance = 30;

ContainerFloatingBehavior::~ContainerFloatingBehavior() {}

void ContainerFloatingBehavior::DoHidingAnimation(
    aura::Window* container,
    ::wm::ScopedHidingAnimationSettings* animation_settings) {
  animation_settings->layer_animation_settings()->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
  gfx::Transform transform;
  transform.Translate(0, kAnimationDistance);
  container->SetTransform(transform);
  container->layer()->SetOpacity(0.f);
}

void ContainerFloatingBehavior::DoShowingAnimation(
    aura::Window* container,
    ui::ScopedLayerAnimationSettings* animation_settings) {
  animation_settings->SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
  animation_settings->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMs));

  container->SetTransform(gfx::Transform());
  container->layer()->SetOpacity(1.0);
}

void ContainerFloatingBehavior::InitializeShowAnimationStartingState(
    aura::Window* container) {
  // TODO(blakeo): preserve the last location of the floating keyboard and use
  // that instead of the top left corner of the screen.
  const int start_x = 0;
  const int start_y = 0;
  const int height = container->bounds().height();

  KeyboardController* controller = KeyboardController::GetInstance();
  container->SetBounds(gfx::Rect(start_x, start_y, kKeyboardWidth, height));

  // UI content container is a child of the keyboard controller container and
  // should always be positioned to the origin (0, 0).
  controller->ui()->GetContentsWindow()->SetBounds(
      gfx::Rect(0, 0, kKeyboardWidth, height));

  gfx::Transform transform;
  transform.Translate(0, kAnimationDistance);
  container->SetTransform(transform);
  container->layer()->SetOpacity(kAnimationStartOrAfterHideOpacity);
}

const gfx::Rect ContainerFloatingBehavior::AdjustSetBoundsRequest(
    const gfx::Rect& display_bounds,
    const gfx::Rect& requested_bounds) const {
  // TODO(blakeo): the keyboard height is currently set by the mandated by
  // the extension, so this value must be preserved. Eventually we'd like to
  // manage the height from here, like the width.
  int height = requested_bounds.height();

  gfx::Rect new_bounds = requested_bounds;
  int left = requested_bounds.x();
  int top = requested_bounds.y();
  int right = left + kKeyboardWidth;
  int bottom = top + height;

  // Prevent dragging off screen
  if (left < 0) {
    left = 0;
    right = kKeyboardWidth;
  }
  if (right >= display_bounds.width()) {
    right = display_bounds.width();
    left = right - kKeyboardWidth;
  }
  if (top < 0) {
    top = 0;
    bottom = height;
  }
  if (bottom >= display_bounds.height()) {
    bottom = display_bounds.height();
    top = bottom - height;
  }

  new_bounds.set_x(left);
  new_bounds.set_y(top);
  new_bounds.set_width(kKeyboardWidth);
  new_bounds.set_height(height);

  return new_bounds;
}

bool ContainerFloatingBehavior::IsOverscrollAllowed() const {
  return false;
}

}  //  namespace keyboard
