// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/container_full_width_behavior.h"

#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/keyboard/container_type.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/wm/core/window_animations.h"

namespace keyboard {

// The virtual keyboard show/hide animation duration.
constexpr int kFullWidthKeyboardAnimationDurationMs = 100;

ContainerFullWidthBehavior::ContainerFullWidthBehavior(
    KeyboardController* controller) {
  controller_ = controller;
}
ContainerFullWidthBehavior::~ContainerFullWidthBehavior() {}

ContainerType ContainerFullWidthBehavior::GetType() const {
  return ContainerType::FULL_WIDTH;
}

void ContainerFullWidthBehavior::DoHidingAnimation(
    aura::Window* container,
    ::wm::ScopedHidingAnimationSettings* animation_settings) {
  animation_settings->layer_animation_settings()->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kFullWidthKeyboardAnimationDurationMs));
  gfx::Transform transform;
  transform.Translate(0, kFullWidthKeyboardAnimationDistance);
  container->SetTransform(transform);
  container->layer()->SetOpacity(0.f);
}

void ContainerFullWidthBehavior::DoShowingAnimation(
    aura::Window* container,
    ui::ScopedLayerAnimationSettings* animation_settings) {
  animation_settings->SetTweenType(gfx::Tween::LINEAR_OUT_SLOW_IN);
  animation_settings->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kFullWidthKeyboardAnimationDurationMs));
  container->SetTransform(gfx::Transform());
  container->layer()->SetOpacity(1.0);
}

void ContainerFullWidthBehavior::InitializeShowAnimationStartingState(
    aura::Window* container) {
  SetCanonicalBounds(container, container->GetRootWindow()->bounds());

  gfx::Transform transform;
  transform.Translate(0, kFullWidthKeyboardAnimationDistance);
  container->SetTransform(transform);
  container->layer()->SetOpacity(kAnimationStartOrAfterHideOpacity);
}

const gfx::Rect ContainerFullWidthBehavior::AdjustSetBoundsRequest(
    const gfx::Rect& display_bounds,
    const gfx::Rect& requested_bounds) {
  gfx::Rect new_bounds;

  // Honors only the height of the request bounds
  const int keyboard_height = requested_bounds.height();

  new_bounds.set_y(display_bounds.height() - keyboard_height);
  new_bounds.set_height(keyboard_height);

  // If shelf is positioned on the left side of screen, x is not 0. In
  // FULL_WIDTH mode, the virtual keyboard should always align with the left
  // edge of the screen. So manually set x to 0 here.
  new_bounds.set_x(0);
  new_bounds.set_width(display_bounds.width());

  return new_bounds;
}

bool ContainerFullWidthBehavior::IsOverscrollAllowed() const {
  // TODO(blakeo): The locked keyboard is essentially its own behavior type and
  // should be refactored as such. Then this will simply return 'true'.
  return controller_ && !controller_->keyboard_locked();
}

void ContainerFullWidthBehavior::SavePosition(const gfx::Rect& keyboard_bounds,
                                              const gfx::Size& screen_size) {
  // No-op. Nothing to save.
}

bool ContainerFullWidthBehavior::IsDragHandle(
    const gfx::Vector2d& offset,
    const gfx::Size& keyboard_size) const {
  return false;
}

void ContainerFullWidthBehavior::HandlePointerEvent(
    const ui::LocatedEvent& event,
    const gfx::Rect& display_bounds) {
  // No-op. Nothing special to do for pointer events.
}

void ContainerFullWidthBehavior::SetCanonicalBounds(
    aura::Window* container,
    const gfx::Rect& display_bounds) {
  const gfx::Rect new_keyboard_bounds =
      AdjustSetBoundsRequest(display_bounds, container->bounds());
  container->SetBounds(new_keyboard_bounds);
}

bool ContainerFullWidthBehavior::TextBlurHidesKeyboard() const {
  return !controller_->keyboard_locked();
}

bool ContainerFullWidthBehavior::BoundsObscureUsableRegion() const {
  return true;
}

bool ContainerFullWidthBehavior::BoundsAffectWorkspaceLayout() const {
  return controller_->keyboard_locked();
}

bool ContainerFullWidthBehavior::SetDraggableArea(const gfx::Rect& rect) {
  return false;
}

}  //  namespace keyboard
