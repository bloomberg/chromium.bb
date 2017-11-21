// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/container_floating_behavior.h"

#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/keyboard/container_type.h"
#include "ui/keyboard/drag_descriptor.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_ui.h"

namespace keyboard {

// Width of the floatin keyboard
constexpr int kKeyboardWidth = 600;

// Length of the animation to show and hide the keyboard.
constexpr int kAnimationDurationMs = 200;

// The opacity of virtual keyboard container when show animation starts or
// hide animation finishes. This cannot be zero because we call Show() on the
// keyboard window before setting the opacity back to 1.0. Since windows are not
// allowed to be shown with zero opacity, we always animate to 0.01 instead.
constexpr float kAnimationStartOrAfterHideOpacity = 0.01f;

// Distance the keyboard moves during the animation
constexpr int kAnimationDistance = 30;

ContainerFloatingBehavior::ContainerFloatingBehavior(
    KeyboardController* controller) {
  controller_ = controller;
}
ContainerFloatingBehavior::~ContainerFloatingBehavior() {}

ContainerType ContainerFloatingBehavior::GetType() const {
  return ContainerType::FLOATING;
}

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
  aura::Window* root_window = container->GetRootWindow();

  SetCanonicalBounds(container, root_window->bounds());

  gfx::Transform transform;
  transform.Translate(0, kAnimationDistance);
  container->SetTransform(transform);
  container->layer()->SetOpacity(kAnimationStartOrAfterHideOpacity);
}

const gfx::Rect ContainerFloatingBehavior::AdjustSetBoundsRequest(
    const gfx::Rect& display_bounds,
    const gfx::Rect& requested_bounds) {
  gfx::Rect keyboard_bounds = requested_bounds;

  // floating keyboard has a fixed width.
  keyboard_bounds.set_width(kKeyboardWidth);

  if (UseDefaultPosition()) {
    // If the keyboard hasn't been shown yet, ignore the request and use
    // default.
    gfx::Point default_location =
        GetPositionForShowingKeyboard(keyboard_bounds.size(), display_bounds);
    keyboard_bounds = gfx::Rect(default_location, keyboard_bounds.size());
  } else {
    // Otherwise, simply make sure that the new bounds are not off the edge of
    // the screen.
    keyboard_bounds =
        ContainKeyboardToScreenBounds(keyboard_bounds, display_bounds);
  }

  return keyboard_bounds;
}

gfx::Rect ContainerFloatingBehavior::ContainKeyboardToScreenBounds(
    const gfx::Rect& keyboard_bounds,
    const gfx::Rect& display_bounds) const {
  int left = keyboard_bounds.x();
  int top = keyboard_bounds.y();
  int right = left + keyboard_bounds.width();
  int bottom = top + keyboard_bounds.height();

  // Prevent keyboard from appearing off screen or overlapping with the edge.
  if (left < 0) {
    left = 0;
    right = keyboard_bounds.width();
  }
  if (right >= display_bounds.width()) {
    right = display_bounds.width();
    left = right - keyboard_bounds.width();
  }
  if (top < 0) {
    top = 0;
    bottom = keyboard_bounds.height();
  }
  if (bottom >= display_bounds.height()) {
    bottom = display_bounds.height();
    top = bottom - keyboard_bounds.height();
  }

  return gfx::Rect(left, top, right - left, bottom - top);
}

bool ContainerFloatingBehavior::IsOverscrollAllowed() const {
  return false;
}

bool ContainerFloatingBehavior::UseDefaultPosition() const {
  // (-1, -1) is used as a sentinel unset value.
  return default_position_.x() == -1;
}

gfx::Point ContainerFloatingBehavior::GetPositionForShowingKeyboard(
    const gfx::Size& keyboard_size,
    const gfx::Rect& display_bounds) const {
  // Start with the last saved position
  gfx::Point position = default_position_;
  if (UseDefaultPosition()) {
    // If there is none, center the keyboard along the bottom of the screen.
    position.set_x(display_bounds.width() - keyboard_size.width() -
                   kDefaultDistanceFromScreenRight);
    position.set_y(display_bounds.height() - keyboard_size.height() -
                   kDefaultDistanceFromScreenBottom);
  }

  // Make sure that this location is valid according to the current size of the
  // screen.
  gfx::Rect keyboard_bounds = gfx::Rect(position, keyboard_size);
  gfx::Rect valid_keyboard_bounds =
      ContainKeyboardToScreenBounds(keyboard_bounds, display_bounds);

  return valid_keyboard_bounds.origin();
}

void ContainerFloatingBehavior::SavePosition(const gfx::Point& position) {
  default_position_ = position;
}

bool ContainerFloatingBehavior::IsDragHandle(
    const gfx::Vector2d& offset,
    const gfx::Size& keyboard_size) const {
  return false;
}

void ContainerFloatingBehavior::HandlePointerEvent(
    bool isMouseButtonPressed,
    const gfx::Vector2d& kb_offset) {
  // Cannot call UI-backed operations without a KeyboardController
  DCHECK(controller_);

  aura::Window* container = controller_->GetContainerWindow();

  const gfx::Rect& keyboard_bounds = container->bounds();

  // Don't handle events if this runs in a partially initialized state.
  if (keyboard_bounds.height() <= 0)
    return;

  if (isMouseButtonPressed &&
      (drag_descriptor_ || IsDragHandle(kb_offset, keyboard_bounds.size()))) {
    if (!drag_descriptor_) {
      // If there is no active drag, start a new one.
      drag_descriptor_.reset(
          new DragDescriptor(keyboard_bounds.origin(), kb_offset));
      container->SetCapture();
    } else {
      // If there is an active drag, use it to determine the new location of the
      // keyboard.
      const gfx::Point original_click_location =
          drag_descriptor_->original_keyboard_location() +
          drag_descriptor_->original_click_offset();
      const gfx::Point current_drag_location =
          keyboard_bounds.origin() + kb_offset;
      const gfx::Vector2d cumulative_drag_offset =
          current_drag_location - original_click_location;
      const gfx::Point new_keyboard_location =
          drag_descriptor_->original_keyboard_location() +
          cumulative_drag_offset;
      const gfx::Rect new_bounds =
          gfx::Rect(new_keyboard_location, keyboard_bounds.size());
      controller_->MoveKeyboard(new_bounds);
    }

    // re-query the container for the new bounds
    SavePosition(container->bounds().origin());
  } else if (drag_descriptor_) {
    // drag has ended
    drag_descriptor_ = nullptr;

    // save the current bounds.
    SavePosition(keyboard_bounds.origin());
    container->ReleaseCapture();
  }
}

void ContainerFloatingBehavior::SetCanonicalBounds(
    aura::Window* container,
    const gfx::Rect& display_bounds) {
  gfx::Size keyboard_size =
      gfx::Size(kKeyboardWidth, container->bounds().height());
  gfx::Point keyboard_location =
      GetPositionForShowingKeyboard(keyboard_size, display_bounds);
  container->SetBounds(gfx::Rect(keyboard_location, keyboard_size));
}

bool ContainerFloatingBehavior::TextBlurHidesKeyboard() const {
  return true;
}

bool ContainerFloatingBehavior::BoundsAffectWorkspaceLayout() const {
  return false;
}

}  //  namespace keyboard
