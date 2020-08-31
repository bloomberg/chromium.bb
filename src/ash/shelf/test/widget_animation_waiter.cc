// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/test/widget_animation_waiter.h"

#include "ash/test/ash_test_base.h"
#include "ui/compositor/layer_animation_delegate.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/widget/widget.h"

namespace ash {

WidgetAnimationWaiter::WidgetAnimationWaiter(views::Widget* widget)
    : target_bounds_(gfx::Rect()), widget_(widget) {
  widget->GetLayer()->GetAnimator()->AddObserver(this);
}

WidgetAnimationWaiter::WidgetAnimationWaiter(views::Widget* widget,
                                             gfx::Rect target_bounds)
    : target_bounds_(target_bounds), widget_(widget) {
  widget->GetLayer()->GetAnimator()->AddObserver(this);
}

WidgetAnimationWaiter::~WidgetAnimationWaiter() {
  widget_->GetLayer()->GetAnimator()->RemoveObserver(this);
}

void WidgetAnimationWaiter::OnLayerAnimationEnded(
    ui::LayerAnimationSequence* sequence) {
  if (!widget_->GetLayer()->GetAnimator()->is_animating() &&
      animation_scheduled_) {
    if (!target_bounds_.IsEmpty()) {
      EXPECT_EQ(widget_->GetWindowBoundsInScreen(), target_bounds_);
      EXPECT_EQ(widget_->GetLayer()->transform(), gfx::Transform());
    }

    is_valid_animation_ = true;
    widget_->GetLayer()->GetAnimator()->RemoveObserver(this);
    run_loop_.Quit();
  }
}
void WidgetAnimationWaiter::OnLayerAnimationAborted(
    ui::LayerAnimationSequence* sequence) {}

void WidgetAnimationWaiter::OnLayerAnimationScheduled(
    ui::LayerAnimationSequence* sequence) {
  animation_scheduled_ = true;
  if (!target_bounds_.IsEmpty())
    EXPECT_NE(widget_->GetLayer()->transform(), gfx::Transform());
}

void WidgetAnimationWaiter::WaitForAnimation() {
  run_loop_.Run();
}

bool WidgetAnimationWaiter::WasValidAnimation() {
  return animation_scheduled_ && is_valid_animation_;
}

}  // namespace ash
