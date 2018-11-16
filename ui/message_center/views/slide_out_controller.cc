// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/slide_out_controller.h"

#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/transform.h"
#include "ui/message_center/public/cpp/message_center_constants.h"

namespace message_center {

SlideOutController::SlideOutController(ui::EventTarget* target,
                                       Delegate* delegate)
    : target_handling_(target, this), delegate_(delegate) {}

SlideOutController::~SlideOutController() {}

void SlideOutController::CaptureControlOpenState() {
  if (!has_swipe_control_)
    return;
  if (mode_ == SlideMode::FULL &&
      fabs(gesture_amount_) >= swipe_control_width_) {
    control_open_state_ = gesture_amount_ < 0
                              ? SwipeControlOpenState::OPEN_ON_RIGHT
                              : SwipeControlOpenState::OPEN_ON_LEFT;
  } else {
    control_open_state_ = SwipeControlOpenState::CLOSED;
  }
}

void SlideOutController::OnGestureEvent(ui::GestureEvent* event) {
  ui::Layer* layer = delegate_->GetSlideOutLayer();
  int width = layer->bounds().width();
  float scroll_amount_for_closing_notification =
      has_swipe_control_ ? swipe_control_width_ + kSwipeCloseMargin
                         : width * 0.5;

  if (event->type() == ui::ET_SCROLL_FLING_START) {
    // The threshold for the fling velocity is computed empirically.
    // The unit is in pixels/second.
    const float kFlingThresholdForClose = 800.f;
    if (mode_ == SlideMode::FULL &&
        fabsf(event->details().velocity_x()) > kFlingThresholdForClose) {
      SlideOutAndClose(event->details().velocity_x());
      event->StopPropagation();
      return;
    }
    CaptureControlOpenState();
    RestoreVisualState();
    delegate_->OnSlideChanged();
    return;
  }

  if (!event->IsScrollGestureEvent())
    return;

  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN) {
    switch (control_open_state_) {
      case SwipeControlOpenState::CLOSED:
        gesture_amount_ = 0.f;
        break;
      case SwipeControlOpenState::OPEN_ON_RIGHT:
        gesture_amount_ = -swipe_control_width_;
        break;
      case SwipeControlOpenState::OPEN_ON_LEFT:
        gesture_amount_ = swipe_control_width_;
        break;
      default:
        NOTREACHED();
    }
  } else if (event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    // The scroll-update events include the incremental scroll amount.
    gesture_amount_ += event->details().scroll_x();

    float scroll_amount;
    float opacity;
    switch (mode_) {
      case SlideMode::FULL:
        scroll_amount = gesture_amount_;
        opacity = 1.f - std::min(fabsf(scroll_amount) / width, 1.f);
        break;
      case SlideMode::NO_SLIDE:
        scroll_amount = 0.f;
        opacity = 1.f;
        break;
      case SlideMode::PARTIALLY:
        if (gesture_amount_ >= 0) {
          scroll_amount = std::min(0.5f * gesture_amount_,
                                   scroll_amount_for_closing_notification);
        } else {
          scroll_amount =
              std::max(0.5f * gesture_amount_,
                       -1.f * scroll_amount_for_closing_notification);
        }
        opacity = 1.f;
        break;
    }

    layer->SetOpacity(opacity);
    gfx::Transform transform;
    transform.Translate(scroll_amount, 0.0);
    layer->SetTransform(transform);
  } else if (event->type() == ui::ET_GESTURE_SCROLL_END) {
    float scrolled_ratio = fabsf(gesture_amount_) / width;
    if (mode_ == SlideMode::FULL &&
        scrolled_ratio >= scroll_amount_for_closing_notification / width) {
      SlideOutAndClose(gesture_amount_);
      event->StopPropagation();
      return;
    }
    CaptureControlOpenState();
    RestoreVisualState();
  }

  delegate_->OnSlideChanged();
  event->SetHandled();
}

void SlideOutController::RestoreVisualState() {
  ui::Layer* layer = delegate_->GetSlideOutLayer();
  // Restore the layer state.
  const int kSwipeRestoreDurationMS = 150;
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kSwipeRestoreDurationMS));
  gfx::Transform transform;
  switch (control_open_state_) {
    case SwipeControlOpenState::CLOSED:
      gesture_amount_ = 0.f;
      break;
    case SwipeControlOpenState::OPEN_ON_RIGHT:
      transform.Translate(-swipe_control_width_, 0);
      break;
    case SwipeControlOpenState::OPEN_ON_LEFT:
      transform.Translate(swipe_control_width_, 0);
      break;
  }
  layer->SetTransform(transform);
  layer->SetOpacity(1.f);
}

void SlideOutController::SlideOutAndClose(int direction) {
  ui::Layer* layer = delegate_->GetSlideOutLayer();
  const int kSwipeOutTotalDurationMS = 150;
  int swipe_out_duration = kSwipeOutTotalDurationMS * layer->opacity();
  ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(swipe_out_duration));
  settings.AddObserver(this);

  gfx::Transform transform;
  int width = layer->bounds().width();
  transform.Translate(direction < 0 ? -width : width, 0.0);
  layer->SetTransform(transform);
  layer->SetOpacity(0.f);
  delegate_->OnSlideChanged();
}

void SlideOutController::OnImplicitAnimationsCompleted() {
  delegate_->OnSlideChanged();

  // OnImplicitAnimationsCompleted is called from BeginMainFrame, so we should
  // delay operation that might result in deletion of LayerTreeHost.
  // https://crbug.com/895883
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&Delegate::OnSlideOut, base::Unretained(delegate_)));
}

void SlideOutController::EnableSwipeControl(int button_count) {
  DCHECK(button_count > 0);
  swipe_control_width_ =
      kSwipeControlButtonSize * button_count +
      kSwipeControlButtonHorizontalMargin * (button_count + 1);
  has_swipe_control_ = true;
}

void SlideOutController::CloseSwipeControl() {
  if (!has_swipe_control_)
    return;
  gesture_amount_ = 0;
  CaptureControlOpenState();
  RestoreVisualState();
}

}  // namespace message_center
