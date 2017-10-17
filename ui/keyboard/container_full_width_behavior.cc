// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/container_full_width_behavior.h"

#include "ui/aura/window.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/wm/core/window_animations.h"

namespace keyboard {

// The virtual keyboard show/hide animation duration.
constexpr int kFullWidthKeyboardAnimationDurationMs = 100;

// The opacity of virtual keyboard container when show animation starts or
// hide animation finishes. This cannot be zero because we call Show() on the
// keyboard window before setting the opacity back to 1.0. Since windows are not
// allowed to be shown with zero opacity, we always animate to 0.01 instead.
constexpr float kAnimationStartOrAfterHideOpacity = 0.01f;

ContainerFullWidthBehavior::~ContainerFullWidthBehavior() {}

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
  gfx::Transform transform;
  transform.Translate(0, kFullWidthKeyboardAnimationDistance);
  container->SetTransform(transform);
  container->layer()->SetOpacity(kAnimationStartOrAfterHideOpacity);
}

}  //  namespace keyboard
