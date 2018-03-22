// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/snap_fling_controller.h"

#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "ui/events/blink/snap_fling_curve.h"

namespace ui {

SnapFlingController::SnapFlingController(SnapFlingClient* client)
    : client_(client), state_(State::kIdle) {}

SnapFlingController::~SnapFlingController() = default;

bool SnapFlingController::FilterEventForSnap(
    const blink::WebInputEvent& event) {
  switch (event.GetType()) {
    case blink::WebInputEvent::kGestureScrollBegin: {
      ClearSnapFling();
      return false;
    }
    // TODO(sunyunjia): Need to update the existing snap curve if the GSU is
    // from a fling boosting event.
    case blink::WebInputEvent::kGestureScrollUpdate:
    case blink::WebInputEvent::kGestureScrollEnd: {
      return state_ == State::kActive || state_ == State::kFinished;
    }
    default:
      return false;
  }
}

void SnapFlingController::ClearSnapFling() {
  if (state_ == State::kActive)
    client_->ScrollEndForSnapFling();

  curve_.reset();
  state_ = State::kIdle;
}

bool SnapFlingController::HandleGestureScrollUpdate(
    const blink::WebGestureEvent& event) {
  DCHECK(state_ != State::kActive && state_ != State::kFinished);
  if (state_ != State::kIdle)
    return false;

  if (event.data.scroll_update.inertial_phase !=
      blink::WebGestureEvent::kMomentumPhase) {
    return false;
  }

  gfx::Vector2dF event_delta(-event.data.scroll_update.delta_x,
                             -event.data.scroll_update.delta_y);
  gfx::Vector2dF ending_displacement =
      SnapFlingCurve::EstimateDisplacement(event_delta);

  gfx::Vector2dF target_offset, start_offset;
  if (!client_->GetSnapFlingInfo(ending_displacement, &start_offset,
                                 &target_offset)) {
    state_ = State::kIgnored;
    return false;
  }

  if (start_offset == target_offset) {
    state_ = State::kFinished;
    return true;
  }

  base::TimeTicks event_time =
      base::TimeTicks() +
      base::TimeDelta::FromSecondsD(event.TimeStampSeconds());
  curve_ =
      std::make_unique<SnapFlingCurve>(start_offset, target_offset, event_time);
  state_ = State::kActive;
  Animate(event_time);
  return true;
}

void SnapFlingController::Animate(base::TimeTicks time) {
  if (state_ != State::kActive)
    return;

  if (curve_->IsFinished()) {
    client_->ScrollEndForSnapFling();
    state_ = State::kFinished;
    return;
  }
  gfx::Vector2dF snapped_delta = curve_->GetScrollDelta(time);
  gfx::Vector2dF current_offset = client_->ScrollByForSnapFling(snapped_delta);
  curve_->UpdateCurrentOffset(current_offset);
  client_->RequestAnimationForSnapFling();
}

void SnapFlingController::SetCurveForTest(
    std::unique_ptr<SnapFlingCurve> curve) {
  curve_ = std::move(curve);
  state_ = State::kActive;
}

}  // namespace ui