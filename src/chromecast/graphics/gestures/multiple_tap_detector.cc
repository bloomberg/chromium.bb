// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/gestures/multiple_tap_detector.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"

namespace chromecast {

namespace {

std::unique_ptr<ui::TouchEvent> MakeCancelEvent(
    const base::TimeTicks& time_stamp,
    const ui::TouchEvent& stashed_press) {
  std::unique_ptr<ui::TouchEvent> rewritten_touch_event =
      std::make_unique<ui::TouchEvent>(ui::ET_TOUCH_CANCELLED, gfx::Point(),
                                       time_stamp,
                                       stashed_press.pointer_details());
  rewritten_touch_event->set_location_f(stashed_press.location_f());
  rewritten_touch_event->set_root_location_f(stashed_press.root_location_f());
  rewritten_touch_event->set_flags(stashed_press.flags());

  return rewritten_touch_event;
}

}  // namespace

MultipleTapDetector::MultipleTapDetector(aura::Window* root_window,
                                         MultipleTapDetectorDelegate* delegate)
    : root_window_(root_window),
      delegate_(delegate),
      enabled_(false),
      tap_state_(MultiTapState::NONE),
      tap_count_(0) {
  root_window->GetHost()->GetEventSource()->AddEventRewriter(this);
}

MultipleTapDetector::~MultipleTapDetector() {
  root_window_->GetHost()->GetEventSource()->RemoveEventRewriter(this);
}

ui::EventRewriteStatus MultipleTapDetector::RewriteEvent(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* rewritten_event) {
  if (!enabled_ || !delegate_ || !event.IsTouchEvent()) {
    return ui::EVENT_REWRITE_CONTINUE;
  }

  const ui::TouchEvent& touch_event = static_cast<const ui::TouchEvent&>(event);
  if (event.type() == ui::ET_TOUCH_PRESSED) {
    // If a press happened again before the minimum inter-tap interval, cancel
    // the detection.
    if (tap_state_ == MultiTapState::INTERVAL_WAIT &&
        (event.time_stamp() - stashed_events_.back().time_stamp()) <
            gesture_detector_config_.double_tap_min_time) {
      stashed_events_.clear();
      TapDetectorStateReset();
      return ui::EVENT_REWRITE_CONTINUE;
    }

    // If the user moved too far from the last tap position, it's not a multi
    // tap.
    if (tap_count_) {
      float distance = (touch_event.location() - last_tap_location_).Length();
      if (distance > gesture_detector_config_.double_tap_slop) {
        TapDetectorStateReset();
        stashed_events_.clear();
        return ui::EVENT_REWRITE_CONTINUE;
      }
    }

    // Otherwise transition into a touched state.
    tap_state_ = MultiTapState::TOUCH;
    last_tap_location_ = touch_event.location();

    // If this is pressed too long, it should be treated as a long-press, and
    // not part of a triple-tap, so set a timer to detect that.
    triple_tap_timer_.Start(
        FROM_HERE, gesture_detector_config_.longpress_timeout, this,
        &MultipleTapDetector::OnLongPressIntervalTimerFired);

    // If we've already gotten one tap, discard this event, only the original
    // tap needs to get through.
    if (tap_count_) {
      return ui::EVENT_REWRITE_DISCARD;
    }

    // Copy the event so we can issue a cancel for it later if this turns out to
    // be a multi-tap.
    stashed_events_.push_back(touch_event);

    return ui::EVENT_REWRITE_CONTINUE;
  }

  // Finger was released while we were waiting for one, count it as a tap.
  if (touch_event.type() == ui::ET_TOUCH_RELEASED &&
      tap_state_ == MultiTapState::TOUCH) {
    tap_state_ = MultiTapState::INTERVAL_WAIT;
    triple_tap_timer_.Start(FROM_HERE,
                            gesture_detector_config_.double_tap_timeout, this,
                            &MultipleTapDetector::OnTapIntervalTimerFired);

    tap_count_++;
    if (tap_count_ == 3) {
      TapDetectorStateReset();
      delegate_->OnTripleTap(touch_event.location());

      // Start issuing cancel events for old presses.
      DCHECK(!stashed_events_.empty());
      *rewritten_event =
          MakeCancelEvent(touch_event.time_stamp(), stashed_events_.front());
      stashed_events_.pop_front();

      if (!stashed_events_.empty())
        return ui::EVENT_REWRITE_DISPATCH_ANOTHER;
      else
        return ui::EVENT_REWRITE_REWRITTEN;
    } else if (tap_count_ > 1) {
      return ui::EVENT_REWRITE_DISCARD;
    }
  }

  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus MultipleTapDetector::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  *new_event =
      MakeCancelEvent(last_event.time_stamp(), stashed_events_.front());
  stashed_events_.pop_front();

  if (!stashed_events_.empty())
    return ui::EVENT_REWRITE_DISPATCH_ANOTHER;
  else
    return ui::EVENT_REWRITE_DISCARD;
}

void MultipleTapDetector::OnTapIntervalTimerFired() {
  // We didn't quite reach a third tap, but a second was reached.
  // So call out the double-tap.
  if (tap_count_ == 2) {
    delegate_->OnDoubleTap(last_tap_location_);

    // Unfortunately we cannot NextDispatchEvent to issue a cancel on the second
    // tap, so we have to manually DispatchEvent to the EventSource.
    // Subsequent EventRewriters in the chain will not see the event.
    if (!stashed_events_.empty()) {
      std::unique_ptr<ui::TouchEvent> cancel_event =
          MakeCancelEvent(base::TimeTicks::Now(), stashed_events_.front());
      DispatchEvent(cancel_event.get());
    }
  }
  TapDetectorStateReset();
  stashed_events_.clear();
}

void MultipleTapDetector::OnLongPressIntervalTimerFired() {
  TapDetectorStateReset();
  stashed_events_.clear();
}

void MultipleTapDetector::TapDetectorStateReset() {
  tap_state_ = MultiTapState::NONE;
  tap_count_ = 0;
  triple_tap_timer_.Stop();
}

void MultipleTapDetector::DispatchEvent(ui::TouchEvent* event) {
  // Turn off triple-tap before re-dispatching to avoid infinite recursion into
  // this detector.
  base::AutoReset<bool> toggle_enable(&enabled_, false);
  DCHECK(!root_window_->GetHost()
              ->dispatcher()
              ->OnEventFromSource(event)
              .dispatcher_destroyed);
}
}  // namespace chromecast
