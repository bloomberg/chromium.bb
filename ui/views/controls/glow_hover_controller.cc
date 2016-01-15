// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/glow_hover_controller.h"

#include "ui/views/view.h"

namespace views {

// Amount to scale the opacity.
static const double kTrackOpacityScale = 0.25;
static const double kHighlightOpacityScale = 1.0;

// How long the hover state takes.
static const int kTrackHoverDurationMs = 400;

GlowHoverController::GlowHoverController(views::View* view)
    : view_(view),
      animation_(this),
      opacity_scale_(kTrackOpacityScale) {
  animation_.set_delegate(this);
}

GlowHoverController::~GlowHoverController() {
}

void GlowHoverController::SetAnimationContainer(
    gfx::AnimationContainer* container) {
  animation_.SetContainer(container);
}

void GlowHoverController::SetLocation(const gfx::Point& location) {
  location_ = location;
  if (ShouldDraw())
    view_->SchedulePaint();
}

void GlowHoverController::Show(Style style) {
  switch (style) {
    case SUBTLE:
      opacity_scale_ = kTrackOpacityScale;
      animation_.SetSlideDuration(kTrackHoverDurationMs);
      animation_.SetTweenType(gfx::Tween::EASE_OUT);
      animation_.Show();
      break;
    case PRONOUNCED:
      opacity_scale_ = kHighlightOpacityScale;
      // Force the end state to show immediately.
      animation_.Show();
      animation_.End();
      break;
  }
}

void GlowHoverController::Hide() {
  animation_.SetTweenType(gfx::Tween::EASE_IN);
  animation_.Hide();
}

void GlowHoverController::HideImmediately() {
  if (ShouldDraw())
    view_->SchedulePaint();
  animation_.Reset();
}

double GlowHoverController::GetAnimationValue() const {
  return animation_.GetCurrentValue();
}

SkAlpha GlowHoverController::GetAlpha() const {
  return static_cast<SkAlpha>(gfx::ToFlooredInt(
      0.5 + animation_.CurrentValueBetween(0., 255 * opacity_scale_)));
}

bool GlowHoverController::ShouldDraw() const {
  return animation_.IsShowing() || animation_.is_animating();
}

void GlowHoverController::AnimationEnded(const gfx::Animation* animation) {
  view_->SchedulePaint();
}

void GlowHoverController::AnimationProgressed(const gfx::Animation* animation) {
  view_->SchedulePaint();
}

}  // namespace views
