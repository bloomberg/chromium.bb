// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/touch_exploration_controller.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_tick_clock.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/rect.h"

#define SET_STATE(state) SetState(state, __func__)
#define VLOG_EVENT(event) if (VLOG_IS_ON(0)) VlogEvent(event, __func__)

namespace ui {

namespace {

// Delay between adjustment sounds.
const base::TimeDelta kSoundDelay = base::TimeDelta::FromMilliseconds(150);

// Delay before corner passthrough activates.
const base::TimeDelta kCornerPassthroughDelay =
    base::TimeDelta::FromMilliseconds(700);

// In ChromeOS, VKEY_LWIN is synonymous for the search key.
const ui::KeyboardCode kChromeOSSearchKey = ui::VKEY_LWIN;
}  // namespace

TouchExplorationController::TouchExplorationController(
    aura::Window* root_window,
    TouchExplorationControllerDelegate* delegate)
    : root_window_(root_window),
      delegate_(delegate),
      state_(NO_FINGERS_DOWN),
      gesture_provider_(new GestureProviderAura(this)),
      prev_state_(NO_FINGERS_DOWN),
      VLOG_on_(true),
      tick_clock_(NULL) {
  CHECK(root_window);
  root_window->GetHost()->GetEventSource()->AddEventRewriter(this);
  InitializeSwipeGestureMaps();
}

TouchExplorationController::~TouchExplorationController() {
  root_window_->GetHost()->GetEventSource()->RemoveEventRewriter(this);
}

ui::EventRewriteStatus TouchExplorationController::RewriteEvent(
    const ui::Event& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  if (!event.IsTouchEvent()) {
    if (event.IsKeyEvent()) {
      const ui::KeyEvent& key_event = static_cast<const ui::KeyEvent&>(event);
      VLOG(0) << "\nKeyboard event: " << key_event.name()
              << "\n Key code: " << key_event.key_code()
              << ", Flags: " << key_event.flags()
              << ", Is char: " << key_event.is_char();
    }
    return ui::EVENT_REWRITE_CONTINUE;
  }
  const ui::TouchEvent& touch_event = static_cast<const ui::TouchEvent&>(event);

  // If the tap timer should have fired by now but hasn't, run it now and
  // stop the timer. This is important so that behavior is consistent with
  // the timestamps of the events, and not dependent on the granularity of
  // the timer.
  if (tap_timer_.IsRunning() &&
      touch_event.time_stamp() - initial_press_->time_stamp() >
          gesture_detector_config_.double_tap_timeout) {
    tap_timer_.Stop();
    OnTapTimerFired();
    // Note: this may change the state. We should now continue and process
    // this event under this new state.
  }

  if (passthrough_timer_.IsRunning() &&
      event.time_stamp() - initial_press_->time_stamp() >
          gesture_detector_config_.longpress_timeout) {
    passthrough_timer_.Stop();
    OnPassthroughTimerFired();
  }

  const ui::EventType type = touch_event.type();
  const gfx::PointF& location = touch_event.location_f();
  const int touch_id = touch_event.touch_id();

  // Always update touch ids and touch locations, so we can use those
  // no matter what state we're in.
  if (type == ui::ET_TOUCH_PRESSED) {
    current_touch_ids_.push_back(touch_id);
    touch_locations_.insert(std::pair<int, gfx::PointF>(touch_id, location));
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    std::vector<int>::iterator it = std::find(
        current_touch_ids_.begin(), current_touch_ids_.end(), touch_id);

    // Can happen if touch exploration is enabled while fingers were down.
    if (it == current_touch_ids_.end())
      return ui::EVENT_REWRITE_CONTINUE;

    current_touch_ids_.erase(it);
    touch_locations_.erase(touch_id);
  } else if (type == ui::ET_TOUCH_MOVED) {
    std::vector<int>::iterator it = std::find(
        current_touch_ids_.begin(), current_touch_ids_.end(), touch_id);

    // Can happen if touch exploration is enabled while fingers were down.
    if (it == current_touch_ids_.end())
      return ui::EVENT_REWRITE_CONTINUE;

    touch_locations_[*it] = location;
  } else {
    NOTREACHED() << "Unexpected event type received: " << event.name();
    return ui::EVENT_REWRITE_CONTINUE;
  }
  VLOG_EVENT(touch_event);

  // In order to avoid accidentally double tapping when moving off the edge
  // of the screen, the state will be rewritten to NoFingersDown.
  if ((type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) &&
      FindEdgesWithinBounds(touch_event.location(), kLeavingScreenEdge) !=
          NO_EDGE) {
    if (VLOG_on_)
      VLOG(0) << "Leaving screen";

    // Indicates to the user that they are leaving the screen.
    delegate_->PlayExitScreenEarcon();

    if (current_touch_ids_.size() == 0) {
      SET_STATE(NO_FINGERS_DOWN);
      if (VLOG_on_) {
        VLOG(0) << "Reset to no fingers in Rewrite event because the touch  "
                   "release or cancel was on the edge of the screen.";
      }
      return ui::EVENT_REWRITE_DISCARD;
    }
  }

  // If the user is in a gesture state, or if there is a possiblity that the
  // user will enter it in the future, we send the event to the gesture
  // provider so it can keep track of the state of the fingers. When the user
  // leaves one of these states, SET_STATE will set the gesture provider to
  // NULL.
  if (gesture_provider_.get()) {
    gesture_provider_->OnTouchEvent(touch_event);
    gesture_provider_->OnTouchEventAck(false);
    ProcessGestureEvents();
  }

  // The rest of the processing depends on what state we're in.
  switch (state_) {
    case NO_FINGERS_DOWN:
      return InNoFingersDown(touch_event, rewritten_event);
    case SINGLE_TAP_PRESSED:
      return InSingleTapPressed(touch_event, rewritten_event);
    case SINGLE_TAP_RELEASED:
    case TOUCH_EXPLORE_RELEASED:
      return InSingleTapOrTouchExploreReleased(touch_event, rewritten_event);
    case DOUBLE_TAP_PENDING:
      return InDoubleTapPending(touch_event, rewritten_event);
    case TOUCH_RELEASE_PENDING:
      return InTouchReleasePending(touch_event, rewritten_event);
    case TOUCH_EXPLORATION:
      return InTouchExploration(touch_event, rewritten_event);
    case GESTURE_IN_PROGRESS:
      return InGestureInProgress(touch_event, rewritten_event);
    case TOUCH_EXPLORE_SECOND_PRESS:
      return InTouchExploreSecondPress(touch_event, rewritten_event);
    case CORNER_PASSTHROUGH:
      return InCornerPassthrough(touch_event, rewritten_event);
    case SLIDE_GESTURE:
      return InSlideGesture(touch_event, rewritten_event);
    case ONE_FINGER_PASSTHROUGH:
      return InOneFingerPassthrough(touch_event, rewritten_event);
    case WAIT_FOR_NO_FINGERS:
      return InWaitForNoFingers(touch_event, rewritten_event);
    case TWO_FINGER_TAP:
      return InTwoFingerTap(touch_event, rewritten_event);
  }
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::NextDispatchEvent(
    const ui::Event& last_event, scoped_ptr<ui::Event>* new_event) {
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InNoFingersDown(
    const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();
  if (type != ui::ET_TOUCH_PRESSED) {
    NOTREACHED() << "Unexpected event type received: " << event.name();
    return ui::EVENT_REWRITE_CONTINUE;
  }

  // If the user enters the screen from the edge then send an earcon.
  int edge = FindEdgesWithinBounds(event.location(), kLeavingScreenEdge);
  if (edge != NO_EDGE)
    delegate_->PlayEnterScreenEarcon();

  int location = FindEdgesWithinBounds(event.location(), kSlopDistanceFromEdge);
  // If the press was at a corner, the user might go into corner passthrough
  // instead.
  bool in_a_bottom_corner =
      (BOTTOM_LEFT_CORNER == location) || (BOTTOM_RIGHT_CORNER == location);
  if (in_a_bottom_corner) {
    passthrough_timer_.Start(
        FROM_HERE,
        gesture_detector_config_.longpress_timeout,
        this,
        &TouchExplorationController::OnPassthroughTimerFired);
  }
  initial_press_.reset(new TouchEvent(event));
  initial_presses_[event.touch_id()] = event.location();
  last_unused_finger_event_.reset(new TouchEvent(event));
  StartTapTimer();
  SET_STATE(SINGLE_TAP_PRESSED);
  return ui::EVENT_REWRITE_DISCARD;
}

ui::EventRewriteStatus TouchExplorationController::InSingleTapPressed(
    const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();

  int location = FindEdgesWithinBounds(event.location(), kMaxDistanceFromEdge);
  bool in_a_bottom_corner =
      (location == BOTTOM_LEFT_CORNER) || (location == BOTTOM_RIGHT_CORNER);
  // If the event is from the initial press and the location is no longer in the
  // corner, then we are not waiting for a corner passthrough anymore.
  if (event.touch_id() == initial_press_->touch_id() && !in_a_bottom_corner) {
    if (passthrough_timer_.IsRunning()) {
      passthrough_timer_.Stop();
      // Since the long press timer has been running, it is possible that the
      // tap timer has timed out before the long press timer has. If the tap
      // timer timeout has elapsed, then fire the tap timer.
      if (event.time_stamp() - initial_press_->time_stamp() >
          gesture_detector_config_.double_tap_timeout) {
        OnTapTimerFired();
      }
    }
  }

  if (type == ui::ET_TOUCH_PRESSED) {
    initial_presses_[event.touch_id()] = event.location();
    SET_STATE(TWO_FINGER_TAP);
    return EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    if (passthrough_timer_.IsRunning())
      passthrough_timer_.Stop();
    if (current_touch_ids_.size() == 0 &&
        event.touch_id() == initial_press_->touch_id()) {
      SET_STATE(SINGLE_TAP_RELEASED);
    } else if (current_touch_ids_.size() == 0) {
      SET_STATE(NO_FINGERS_DOWN);
    }
    return EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_MOVED) {
    float distance = (event.location() - initial_press_->location()).Length();
    // If the user does not move far enough from the original position, then the
    // resulting movement should not be considered to be a deliberate gesture or
    // touch exploration.
    if (distance <= gesture_detector_config_.touch_slop)
      return EVENT_REWRITE_DISCARD;

    float delta_time =
        (event.time_stamp() - initial_press_->time_stamp()).InSecondsF();
    float velocity = distance / delta_time;
    if (VLOG_on_) {
      VLOG(0) << "\n Delta time: " << delta_time << "\n Distance: " << distance
              << "\n Velocity of click: " << velocity
              << "\n Minimum swipe velocity: "
              << gesture_detector_config_.minimum_swipe_velocity;
    }
    // Change to slide gesture if the slide occurred at the right edge.
    int edge = FindEdgesWithinBounds(event.location(), kMaxDistanceFromEdge);
    if (edge & RIGHT_EDGE && edge != BOTTOM_RIGHT_CORNER) {
      SET_STATE(SLIDE_GESTURE);
      return InSlideGesture(event, rewritten_event);
    }

    // If the user moves fast enough from the initial touch location, start
    // gesture detection. Otherwise, jump to the touch exploration mode early.
    if (velocity > gesture_detector_config_.minimum_swipe_velocity) {
      SET_STATE(GESTURE_IN_PROGRESS);
      return InGestureInProgress(event, rewritten_event);
    }
    EnterTouchToMouseMode();
    SET_STATE(TOUCH_EXPLORATION);
    return InTouchExploration(event, rewritten_event);
  }
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus
TouchExplorationController::InSingleTapOrTouchExploreReleased(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();
  // If there is more than one finger down, then discard to wait until no
  // fingers are down.
  if (current_touch_ids_.size() > 1) {
    SET_STATE(WAIT_FOR_NO_FINGERS);
    return ui::EVENT_REWRITE_DISCARD;
  }
  if (type == ui::ET_TOUCH_PRESSED) {
    // If there is no touch exploration yet, we can't send a click, so discard.
    if (!last_touch_exploration_) {
      tap_timer_.Stop();
      return ui::EVENT_REWRITE_DISCARD;
    }
    // This is the second tap in a double-tap (or double tap-hold).
    // We set the tap timer. If it fires before the user lifts their finger,
    // one-finger passthrough begins. Otherwise, there is a touch press and
    // release at the location of the last touch exploration.
    SET_STATE(DOUBLE_TAP_PENDING);
    // The old tap timer (from the initial click) is stopped if it is still
    // going, and the new one is set.
    tap_timer_.Stop();
    StartTapTimer();
    // This will update as the finger moves before a possible passthrough, and
    // will determine the offset.
    last_unused_finger_event_.reset(new ui::TouchEvent(event));
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_RELEASED && !last_touch_exploration_) {
    // If the previous press was discarded, we need to also handle its
    // release.
    if (current_touch_ids_.size() == 0) {
      SET_STATE(NO_FINGERS_DOWN);
    }
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_MOVED) {
    return ui::EVENT_REWRITE_DISCARD;
  }
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InDoubleTapPending(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();
  if (type == ui::ET_TOUCH_PRESSED) {
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_MOVED) {
    // If the user moves far enough from the initial touch location (outside
    // the "slop" region, jump to passthrough mode early.
    float delta = (event.location() - initial_press_->location()).Length();
    if (delta > gesture_detector_config_.touch_slop) {
      tap_timer_.Stop();
      OnTapTimerFired();
    }
    return EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    if (current_touch_ids_.size() != 0)
      return EVENT_REWRITE_DISCARD;

    scoped_ptr<ui::TouchEvent> touch_press;
    touch_press.reset(new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
                                         last_touch_exploration_->location(),
                                         initial_press_->touch_id(),
                                         event.time_stamp()));
    DispatchEvent(touch_press.get());

    rewritten_event->reset(
        new ui::TouchEvent(ui::ET_TOUCH_RELEASED,
                           last_touch_exploration_->location(),
                           initial_press_->touch_id(),
                           event.time_stamp()));
    (*rewritten_event)->set_flags(event.flags());
    SET_STATE(NO_FINGERS_DOWN);
    return ui::EVENT_REWRITE_REWRITTEN;
  }
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InTouchReleasePending(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();
  if (type == ui::ET_TOUCH_PRESSED || type == ui::ET_TOUCH_MOVED) {
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    if (current_touch_ids_.size() != 0)
      return EVENT_REWRITE_DISCARD;

    rewritten_event->reset(
        new ui::TouchEvent(ui::ET_TOUCH_RELEASED,
                           last_touch_exploration_->location(),
                           initial_press_->touch_id(),
                           event.time_stamp()));
    (*rewritten_event)->set_flags(event.flags());
    SET_STATE(NO_FINGERS_DOWN);
    return ui::EVENT_REWRITE_REWRITTEN;
  }
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InTouchExploration(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();
  if (type == ui::ET_TOUCH_PRESSED) {
    // Handle split-tap.
    initial_press_.reset(new TouchEvent(event));
    tap_timer_.Stop();
    rewritten_event->reset(
        new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
                           last_touch_exploration_->location(),
                           event.touch_id(),
                           event.time_stamp()));
    (*rewritten_event)->set_flags(event.flags());
    SET_STATE(TOUCH_EXPLORE_SECOND_PRESS);
    return ui::EVENT_REWRITE_REWRITTEN;
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    initial_press_.reset(new TouchEvent(event));
    StartTapTimer();
    SET_STATE(TOUCH_EXPLORE_RELEASED);
  } else if (type != ui::ET_TOUCH_MOVED) {
    NOTREACHED();
    return ui::EVENT_REWRITE_CONTINUE;
  }

  // Rewrite as a mouse-move event.
  *rewritten_event = CreateMouseMoveEvent(event.location(), event.flags());
  last_touch_exploration_.reset(new TouchEvent(event));
  return ui::EVENT_REWRITE_REWRITTEN;
}

ui::EventRewriteStatus TouchExplorationController::InGestureInProgress(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  // The events were sent to the gesture provider in RewriteEvent already.
  // If no gesture is registered before the tap timer times out, the state
  // will change to "wait for no fingers down" or "touch exploration" depending
  // on the number of fingers down, and this function will stop being called.
  if (current_touch_ids_.size() == 0) {
    SET_STATE(NO_FINGERS_DOWN);
  }
  return ui::EVENT_REWRITE_DISCARD;
}

ui::EventRewriteStatus TouchExplorationController::InCornerPassthrough(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  ui::EventType type = event.type();

  // If the first finger has left the corner, then exit passthrough.
  if (event.touch_id() == initial_press_->touch_id()) {
    int edges = FindEdgesWithinBounds(event.location(), kSlopDistanceFromEdge);
    bool in_a_bottom_corner = (edges == BOTTOM_LEFT_CORNER) ||
                              (edges == BOTTOM_RIGHT_CORNER);
    if (type == ui::ET_TOUCH_MOVED && in_a_bottom_corner)
      return ui::EVENT_REWRITE_DISCARD;

    if (current_touch_ids_.size() == 0) {
      SET_STATE(NO_FINGERS_DOWN);
      return ui::EVENT_REWRITE_DISCARD;
    }
    SET_STATE(WAIT_FOR_NO_FINGERS);
    return ui::EVENT_REWRITE_DISCARD;
  }

  rewritten_event->reset(new ui::TouchEvent(
      type, event.location(), event.touch_id(), event.time_stamp()));
  (*rewritten_event)->set_flags(event.flags());

  if (current_touch_ids_.size() == 0)
    SET_STATE(NO_FINGERS_DOWN);

  return ui::EVENT_REWRITE_REWRITTEN;
}

ui::EventRewriteStatus TouchExplorationController::InOneFingerPassthrough(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  if (event.touch_id() != initial_press_->touch_id()) {
    if (current_touch_ids_.size() == 0) {
      SET_STATE(NO_FINGERS_DOWN);
    }
    return ui::EVENT_REWRITE_DISCARD;
  }
  rewritten_event->reset(
      new ui::TouchEvent(event.type(),
                         event.location() - passthrough_offset_,
                         event.touch_id(),
                         event.time_stamp()));

  (*rewritten_event)->set_flags(event.flags());
  if (current_touch_ids_.size() == 0) {
    SET_STATE(NO_FINGERS_DOWN);
  }
  return ui::EVENT_REWRITE_REWRITTEN;
}

ui::EventRewriteStatus TouchExplorationController::InTouchExploreSecondPress(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  ui::EventType type = event.type();
  gfx::PointF location = event.location_f();
  if (type == ui::ET_TOUCH_PRESSED) {
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_MOVED) {
    // Currently this is a discard, but could be something like rotor
    // in the future.
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    // If the touch exploration finger is lifted, there is no option to return
    // to touch explore anymore. The remaining finger acts as a pending
    // tap or long tap for the last touch explore location.
    if (event.touch_id() == last_touch_exploration_->touch_id()){
      SET_STATE(TOUCH_RELEASE_PENDING);
      return EVENT_REWRITE_DISCARD;
    }

    // Continue to release the touch only if the touch explore finger is the
    // only finger remaining.
    if (current_touch_ids_.size() != 1)
      return EVENT_REWRITE_DISCARD;

    // Rewrite at location of last touch exploration.
    rewritten_event->reset(
        new ui::TouchEvent(ui::ET_TOUCH_RELEASED,
                           last_touch_exploration_->location(),
                           initial_press_->touch_id(),
                           event.time_stamp()));
    (*rewritten_event)->set_flags(event.flags());
    SET_STATE(TOUCH_EXPLORATION);
    EnterTouchToMouseMode();
    return ui::EVENT_REWRITE_REWRITTEN;
  }
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InWaitForNoFingers(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  if (current_touch_ids_.size() == 0)
    SET_STATE(NO_FINGERS_DOWN);
  return EVENT_REWRITE_DISCARD;
}

void TouchExplorationController::PlaySoundForTimer() {
  delegate_->PlayVolumeAdjustEarcon();
}

ui::EventRewriteStatus TouchExplorationController::InSlideGesture(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  // The timer should not fire when sliding.
  tap_timer_.Stop();

  ui::EventType type = event.type();
  // If additional fingers are added before a swipe gesture has been registered,
  // then wait until all fingers have been lifted.
  if (type == ui::ET_TOUCH_PRESSED ||
      event.touch_id() != initial_press_->touch_id()) {
    if (sound_timer_.IsRunning())
      sound_timer_.Stop();
    SET_STATE(WAIT_FOR_NO_FINGERS);
    return EVENT_REWRITE_DISCARD;
  }

  // There should not be more than one finger down.
  DCHECK(current_touch_ids_.size() <= 1);

  // Allows user to return to the edge to adjust the sound if they have left the
  // boundaries.
  int edge = FindEdgesWithinBounds(event.location(), kSlopDistanceFromEdge);
  if (!(edge & RIGHT_EDGE) && (type != ui::ET_TOUCH_RELEASED)) {
    if (sound_timer_.IsRunning()) {
      sound_timer_.Stop();
    }
    return EVENT_REWRITE_DISCARD;
  }

  // This can occur if the user leaves the screen edge and then returns to it to
  // continue adjusting the sound.
  if (!sound_timer_.IsRunning()) {
    sound_timer_.Start(FROM_HERE,
                       kSoundDelay,
                       this,
                       &ui::TouchExplorationController::PlaySoundForTimer);
    delegate_->PlayVolumeAdjustEarcon();
  }

  if (current_touch_ids_.size() == 0) {
    SET_STATE(NO_FINGERS_DOWN);
  }
  return ui::EVENT_REWRITE_DISCARD;
}

ui::EventRewriteStatus TouchExplorationController::InTwoFingerTap(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  ui::EventType type = event.type();
  if (type == ui::ET_TOUCH_PRESSED) {
    // This is now a three finger gesture.
    SET_STATE(GESTURE_IN_PROGRESS);
    return ui::EVENT_REWRITE_DISCARD;
  }

  if (type == ui::ET_TOUCH_MOVED) {
    // Determine if it was a swipe.
    gfx::Point original_location = initial_presses_[event.touch_id()];
    float distance = (event.location() - original_location).Length();
    // If the user moves too far from the original position, consider the
    // movement a swipe.
    if (distance > gesture_detector_config_.touch_slop) {
      SET_STATE(GESTURE_IN_PROGRESS);
    }
    return ui::EVENT_REWRITE_DISCARD;
  }

  if (current_touch_ids_.size() != 0)
    return ui::EVENT_REWRITE_DISCARD;

  if (type == ui::ET_TOUCH_RELEASED) {
    // In ChromeVox, pressing control will stop ChromeVox from speaking.
    ui::KeyEvent control_down(
        ui::ET_KEY_PRESSED, ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN);
    ui::KeyEvent control_up(ui::ET_KEY_RELEASED, ui::VKEY_CONTROL, ui::EF_NONE);

    DispatchEvent(&control_down);
    DispatchEvent(&control_up);
    SET_STATE(NO_FINGERS_DOWN);
    return ui::EVENT_REWRITE_DISCARD;
  }
  return ui::EVENT_REWRITE_DISCARD;
}

base::TimeDelta TouchExplorationController::Now() {
  if (tick_clock_) {
    // This is the same as what EventTimeForNow() does, but here we do it
    // with a clock that can be replaced with a simulated clock for tests.
    return base::TimeDelta::FromInternalValue(
        tick_clock_->NowTicks().ToInternalValue());
  }
  return ui::EventTimeForNow();
}

void TouchExplorationController::StartTapTimer() {
  tap_timer_.Start(FROM_HERE,
                   gesture_detector_config_.double_tap_timeout,
                   this,
                   &TouchExplorationController::OnTapTimerFired);
}

void TouchExplorationController::OnTapTimerFired() {
  switch (state_) {
    case SINGLE_TAP_RELEASED:
      SET_STATE(NO_FINGERS_DOWN);
      break;
    case TOUCH_EXPLORE_RELEASED:
      SET_STATE(NO_FINGERS_DOWN);
      last_touch_exploration_.reset(new TouchEvent(*initial_press_));
      return;
    case DOUBLE_TAP_PENDING: {
      SET_STATE(ONE_FINGER_PASSTHROUGH);
      passthrough_offset_ = last_unused_finger_event_->location() -
                            last_touch_exploration_->location();
      scoped_ptr<ui::TouchEvent> passthrough_press(
          new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
                             last_touch_exploration_->location(),
                             last_unused_finger_event_->touch_id(),
                             Now()));
      DispatchEvent(passthrough_press.get());
      return;
    }
    case SINGLE_TAP_PRESSED:
      if (passthrough_timer_.IsRunning())
        return;
    case GESTURE_IN_PROGRESS:
      // If only one finger is down, go into touch exploration.
      if (current_touch_ids_.size() == 1) {
        SET_STATE(TOUCH_EXPLORATION);
        break;
      }
      // Otherwise wait for all fingers to be lifted.
      SET_STATE(WAIT_FOR_NO_FINGERS);
      return;
    case TWO_FINGER_TAP:
      SET_STATE(WAIT_FOR_NO_FINGERS);
      break;
    default:
      return;
  }
  EnterTouchToMouseMode();
  scoped_ptr<ui::Event> mouse_move =
      CreateMouseMoveEvent(initial_press_->location(), initial_press_->flags());
  DispatchEvent(mouse_move.get());
  last_touch_exploration_.reset(new TouchEvent(*initial_press_));
}

void TouchExplorationController::OnPassthroughTimerFired() {
  // The passthrough timer will only fire if if the user has held a finger in
  // one of the passthrough corners for the duration of the passthrough timeout.

  // Check that initial press isn't null. Also a check that if the initial
  // corner press was released, then it should not be in corner passthrough.
  if (!initial_press_ ||
      touch_locations_.find(initial_press_->touch_id()) !=
          touch_locations_.end()) {
    LOG(ERROR) << "No initial press or the initial press has been released.";
  }

  gfx::Point location =
      ToRoundedPoint(touch_locations_[initial_press_->touch_id()]);
  int corner = FindEdgesWithinBounds(location, kSlopDistanceFromEdge);
  if (corner != BOTTOM_LEFT_CORNER && corner != BOTTOM_RIGHT_CORNER)
    return;

  if (sound_timer_.IsRunning())
    sound_timer_.Stop();
  delegate_->PlayPassthroughEarcon();
  SET_STATE(CORNER_PASSTHROUGH);
  return;
}

void TouchExplorationController::DispatchEvent(ui::Event* event) {
  ui::EventDispatchDetails result ALLOW_UNUSED =
      root_window_->GetHost()->dispatcher()->OnEventFromSource(event);
}

// This is an override for a function that is only called for timer-based events
// like long press. Events that are created synchronously as a result of
// certain touch events are added to the vector accessible via
// GetAndResetPendingGestures(). We only care about swipes (which are created
// synchronously), so we ignore this callback.
void TouchExplorationController::OnGestureEvent(ui::GestureEvent* gesture) {
}

void TouchExplorationController::ProcessGestureEvents() {
  scoped_ptr<ScopedVector<ui::GestureEvent> > gestures(
      gesture_provider_->GetAndResetPendingGestures());
  if (gestures) {
    for (ScopedVector<GestureEvent>::iterator i = gestures->begin();
         i != gestures->end();
         ++i) {
      if ((*i)->type() == ui::ET_GESTURE_SWIPE &&
          state_ == GESTURE_IN_PROGRESS) {
        OnSwipeEvent(*i);
        // The tap timer to leave gesture state is ended, and we now wait for
        // all fingers to be released.
        tap_timer_.Stop();
        SET_STATE(WAIT_FOR_NO_FINGERS);
        return;
      }
      if (state_ == SLIDE_GESTURE && (*i)->IsScrollGestureEvent()) {
        SideSlideControl(*i);
      }
    }
  }
}

void TouchExplorationController::SideSlideControl(ui::GestureEvent* gesture) {
  ui::EventType type = gesture->type();

  if (type == ET_GESTURE_SCROLL_BEGIN) {
    delegate_->PlayVolumeAdjustEarcon();
  }

  if (type == ET_GESTURE_SCROLL_END) {
    if (sound_timer_.IsRunning())
      sound_timer_.Stop();
    delegate_->PlayVolumeAdjustEarcon();
  }

  // If the user is in the corner of the right side of the screen, the volume
  // will be automatically set to 100% or muted depending on which corner they
  // are in. Otherwise, the user will be able to adjust the volume by sliding
  // their finger along the right side of the screen. Volume is relative to
  // where they are on the right side of the screen.
  gfx::Point location = gesture->location();
  int edge = FindEdgesWithinBounds(location, kSlopDistanceFromEdge);
  if (!(edge & RIGHT_EDGE))
    return;

  if (edge & TOP_EDGE) {
    delegate_->SetOutputLevel(100);
    return;
  }
  if (edge & BOTTOM_EDGE) {
    delegate_->SetOutputLevel(0);
    return;
  }

  location = gesture->location();
  root_window_->GetHost()->ConvertPointFromNativeScreen(&location);
  float volume_adjust_height =
      root_window_->bounds().height() - 2 * kMaxDistanceFromEdge;
  float ratio = (location.y() - kMaxDistanceFromEdge) / volume_adjust_height;
  float volume = 100 - 100 * ratio;
  if (VLOG_on_) {
    VLOG(0) << "\n Volume = " << volume
            << "\n Location = " << location.ToString()
            << "\n Bounds = " << root_window_->bounds().right();
  }
  delegate_->SetOutputLevel(int(volume));
}

void TouchExplorationController::OnSwipeEvent(ui::GestureEvent* swipe_gesture) {
  // A swipe gesture contains details for the direction in which the swipe
  // occurred. TODO(evy) : Research which swipe results users most want and
  // remap these swipes to the best events. Hopefully in the near future
  // there will also be a menu for users to pick custom mappings.
  GestureEventDetails event_details = swipe_gesture->details();
  int num_fingers = event_details.touch_points();
  if(VLOG_on_)
    VLOG(0) << "\nSwipe with " << num_fingers << " fingers.";

  if (num_fingers > 4)
    return;

  if (event_details.swipe_left() &&
      !left_swipe_gestures_[num_fingers].is_null()) {
    left_swipe_gestures_[num_fingers].Run();
  } else if (event_details.swipe_right() &&
             !right_swipe_gestures_[num_fingers].is_null()) {
    right_swipe_gestures_[num_fingers].Run();
  } else if (event_details.swipe_up() &&
             !up_swipe_gestures_[num_fingers].is_null()) {
    up_swipe_gestures_[num_fingers].Run();
  } else if (event_details.swipe_down() &&
             !down_swipe_gestures_[num_fingers].is_null()) {
    down_swipe_gestures_[num_fingers].Run();
  }
}

int TouchExplorationController::FindEdgesWithinBounds(gfx::Point point,
                                                      float bounds) {
  // Since GetBoundsInScreen is in DIPs but point is not, then point needs to be
  // converted.
  root_window_->GetHost()->ConvertPointFromNativeScreen(&point);
  gfx::Rect window = root_window_->GetBoundsInScreen();

  float left_edge_limit = window.x() + bounds;
  float right_edge_limit = window.right() - bounds;
  float top_edge_limit = window.y() + bounds;
  float bottom_edge_limit = window.bottom() - bounds;

  // Bitwise manipulation in order to determine where on the screen the point
  // lies. If more than one bit is turned on, then it is a corner where the two
  // bit/edges intersect. Otherwise, if no bits are turned on, the point must be
  // in the center of the screen.
  int result = NO_EDGE;
  if (point.x() < left_edge_limit)
    result |= LEFT_EDGE;
  if (point.x() > right_edge_limit)
    result |= RIGHT_EDGE;
  if (point.y() < top_edge_limit)
    result |= TOP_EDGE;
  if (point.y() > bottom_edge_limit)
    result |= BOTTOM_EDGE;
  return result;
}

void TouchExplorationController::DispatchShiftSearchKeyEvent(
    const ui::KeyboardCode third_key) {
  // In order to activate the shortcut shift+search+<arrow key>
  // three KeyPressed events must be dispatched in succession along
  // with three KeyReleased events.

  ui::KeyEvent shift_down(
      ui::ET_KEY_PRESSED, ui::VKEY_SHIFT, ui::EF_SHIFT_DOWN);
  ui::KeyEvent search_down(
      ui::ET_KEY_PRESSED, kChromeOSSearchKey, ui::EF_SHIFT_DOWN);
  ui::KeyEvent third_key_down(ui::ET_KEY_PRESSED, third_key, ui::EF_SHIFT_DOWN);

  ui::KeyEvent third_key_up(ui::ET_KEY_RELEASED, third_key, ui::EF_SHIFT_DOWN);
  ui::KeyEvent search_up(
      ui::ET_KEY_RELEASED, kChromeOSSearchKey, ui::EF_SHIFT_DOWN);
  ui::KeyEvent shift_up(ui::ET_KEY_RELEASED, ui::VKEY_SHIFT, ui::EF_NONE);

  DispatchEvent(&shift_down);
  DispatchEvent(&search_down);
  DispatchEvent(&third_key_down);
  DispatchEvent(&third_key_up);
  DispatchEvent(&search_up);
  DispatchEvent(&shift_up);
}

base::Closure TouchExplorationController::BindShiftSearchKeyEvent(
    const ui::KeyboardCode third_key) {
  return base::Bind(&TouchExplorationController::DispatchShiftSearchKeyEvent,
                    base::Unretained(this),
                    third_key);
}

void TouchExplorationController::DispatchKeyWithFlags(
    const ui::KeyboardCode key,
    int flags) {
  ui::KeyEvent key_down(ui::ET_KEY_PRESSED, key, flags);
  ui::KeyEvent key_up(ui::ET_KEY_RELEASED, key, flags);
  DispatchEvent(&key_down);
  DispatchEvent(&key_up);
  if(VLOG_on_) {
    VLOG(0) << "\nKey down: key code : " << key_down.key_code()
            << ", flags: " << key_down.flags()
            << "\nKey up: key code : " << key_up.key_code()
            << ", flags: " << key_up.flags();
  }
}

base::Closure TouchExplorationController::BindKeyEventWithFlags(
    const ui::KeyboardCode key,
    int flags) {
  return base::Bind(&TouchExplorationController::DispatchKeyWithFlags,
                    base::Unretained(this),
                    key,
                    flags);
}

scoped_ptr<ui::Event> TouchExplorationController::CreateMouseMoveEvent(
    const gfx::PointF& location,
    int flags) {
  // The "synthesized" flag should be set on all events that don't have a
  // backing native event.
  flags |= ui::EF_IS_SYNTHESIZED;

  // This flag is used to identify mouse move events that were generated from
  // touch exploration in Chrome code.
  flags |= ui::EF_TOUCH_ACCESSIBILITY;

  // TODO(dmazzoni) http://crbug.com/391008 - get rid of this hack.
  // This is a short-term workaround for the limitation that we're using
  // the ChromeVox content script to process touch exploration events, but
  // ChromeVox needs a way to distinguish between a real mouse move and a
  // mouse move generated from touch exploration, so we have touch exploration
  // pretend that the command key was down (which becomes the "meta" key in
  // JavaScript). We can remove this hack when the ChromeVox content script
  // goes away and native accessibility code sends a touch exploration
  // event to the new ChromeVox background page via the automation api.
  flags |= ui::EF_COMMAND_DOWN;

  return scoped_ptr<ui::Event>(
      new ui::MouseEvent(ui::ET_MOUSE_MOVED, location, location, flags, 0));
}

void TouchExplorationController::EnterTouchToMouseMode() {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window_);
  if (cursor_client && !cursor_client->IsMouseEventsEnabled())
    cursor_client->EnableMouseEvents();
  if (cursor_client && cursor_client->IsCursorVisible())
    cursor_client->HideCursor();
}

void TouchExplorationController::SetState(State new_state,
                                          const char* function_name) {
  state_ = new_state;
  VlogState(function_name);
  // These are the states the user can be in that will never result in a
  // gesture before the user returns to NO_FINGERS_DOWN. Therefore, if the
  // gesture provider still exists, it's reset to NULL until the user returns
  // to NO_FINGERS_DOWN.
  switch (new_state) {
    case SINGLE_TAP_RELEASED:
    case TOUCH_EXPLORE_RELEASED:
    case DOUBLE_TAP_PENDING:
    case TOUCH_RELEASE_PENDING:
    case TOUCH_EXPLORATION:
    case TOUCH_EXPLORE_SECOND_PRESS:
    case ONE_FINGER_PASSTHROUGH:
    case CORNER_PASSTHROUGH:
    case WAIT_FOR_NO_FINGERS:
      if (gesture_provider_.get())
        gesture_provider_.reset(NULL);
      break;
    case NO_FINGERS_DOWN:
      gesture_provider_.reset(new GestureProviderAura(this));
      if (sound_timer_.IsRunning())
        sound_timer_.Stop();
      tap_timer_.Stop();
      break;
    case SINGLE_TAP_PRESSED:
    case GESTURE_IN_PROGRESS:
    case SLIDE_GESTURE:
    case TWO_FINGER_TAP:
      break;
  }
}

void TouchExplorationController::VlogState(const char* function_name) {
  if (!VLOG_on_)
    return;
  if (prev_state_ == state_)
    return;
  prev_state_ = state_;
  const char* state_string = EnumStateToString(state_);
  VLOG(0) << "\n Function name: " << function_name
          << "\n State: " << state_string;
}

void TouchExplorationController::VlogEvent(const ui::TouchEvent& touch_event,
                                           const char* function_name) {
  if (!VLOG_on_)
    return;

  if (prev_event_ != NULL &&
      prev_event_->type() == touch_event.type() &&
      prev_event_->touch_id() == touch_event.touch_id()){
    return;
  }
  // The above statement prevents events of the same type and id from being
  // printed in a row. However, if two fingers are down, they would both be
  // moving and alternating printing move events unless we check for this.
  if (prev_event_ != NULL &&
      prev_event_->type() == ET_TOUCH_MOVED &&
      touch_event.type() == ET_TOUCH_MOVED){
    return;
  }

  const std::string& type = touch_event.name();
  const gfx::PointF& location = touch_event.location_f();
  const int touch_id = touch_event.touch_id();

  VLOG(0) << "\n Function name: " << function_name
          << "\n Event Type: " << type
          << "\n Location: " << location.ToString()
          << "\n Touch ID: " << touch_id;
  prev_event_.reset(new TouchEvent(touch_event));
}

const char* TouchExplorationController::EnumStateToString(State state) {
  switch (state) {
    case NO_FINGERS_DOWN:
      return "NO_FINGERS_DOWN";
    case SINGLE_TAP_PRESSED:
      return "SINGLE_TAP_PRESSED";
    case SINGLE_TAP_RELEASED:
      return "SINGLE_TAP_RELEASED";
    case TOUCH_EXPLORE_RELEASED:
      return "TOUCH_EXPLORE_RELEASED";
    case DOUBLE_TAP_PENDING:
      return "DOUBLE_TAP_PENDING";
    case TOUCH_RELEASE_PENDING:
      return "TOUCH_RELEASE_PENDING";
    case TOUCH_EXPLORATION:
      return "TOUCH_EXPLORATION";
    case GESTURE_IN_PROGRESS:
      return "GESTURE_IN_PROGRESS";
    case TOUCH_EXPLORE_SECOND_PRESS:
      return "TOUCH_EXPLORE_SECOND_PRESS";
    case CORNER_PASSTHROUGH:
      return "CORNER_PASSTHROUGH";
    case SLIDE_GESTURE:
      return "SLIDE_GESTURE";
    case ONE_FINGER_PASSTHROUGH:
      return "ONE_FINGER_PASSTHROUGH";
    case WAIT_FOR_NO_FINGERS:
      return "WAIT_FOR_NO_FINGERS";
    case TWO_FINGER_TAP:
      return "TWO_FINGER_TAP";
  }
  return "Not a state";
}

// TODO(evy, lisayin) : Just call abstracted methods on the delegate (e.g.
// Swipe(Direction direction, int num_fingers)), and add the DispatchXYZ
// methods to the delegate. Avoid the middle step of dispatching keys at all,
// and simply have ChromeVox/ChromeOS complete the required action.

void TouchExplorationController::InitializeSwipeGestureMaps() {
  // Gestures with one finger are used for navigation.
  left_swipe_gestures_[1] = BindShiftSearchKeyEvent(ui::VKEY_LEFT);
  right_swipe_gestures_[1] = BindShiftSearchKeyEvent(ui::VKEY_RIGHT);
  up_swipe_gestures_[1] = BindShiftSearchKeyEvent(ui::VKEY_UP);
  down_swipe_gestures_[1] = BindShiftSearchKeyEvent(ui::VKEY_DOWN);

  // Gestures with two fingers.
  left_swipe_gestures_[2] =
      BindKeyEventWithFlags(ui::VKEY_BROWSER_BACK, ui::EF_NONE);
  right_swipe_gestures_[2] =
      BindKeyEventWithFlags(ui::VKEY_BROWSER_FORWARD, ui::EF_NONE);
  // Jump to top.
  up_swipe_gestures_[2] = BindShiftSearchKeyEvent(ui::VKEY_A);
  // Read from here.
  down_swipe_gestures_[2] = BindShiftSearchKeyEvent(ui::VKEY_R);

  // Gestures with three fingers switch tabs left/right and scroll up/down.
  left_swipe_gestures_[3] = BindKeyEventWithFlags(
      ui::VKEY_TAB, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  right_swipe_gestures_[3] =
      BindKeyEventWithFlags(ui::VKEY_TAB, ui::EF_CONTROL_DOWN);
  up_swipe_gestures_[3] = BindKeyEventWithFlags(ui::VKEY_NEXT, ui::EF_NONE);
  down_swipe_gestures_[3] = BindKeyEventWithFlags(ui::VKEY_PRIOR, ui::EF_NONE);

  // Gestures with four fingers should probably eventually be used for rare
  // needs that are hard to access through menus.
  // Note that brightness levels are here because they can be important for low
  // vision users. However, none of these mappings are permanent.
  left_swipe_gestures_[4] =
      BindKeyEventWithFlags(ui::VKEY_BRIGHTNESS_DOWN, ui::EF_NONE);
  right_swipe_gestures_[4] =
      BindKeyEventWithFlags(VKEY_BRIGHTNESS_UP, ui::EF_NONE);
  up_swipe_gestures_[4] = BindKeyEventWithFlags(VKEY_BROWSER_HOME, ui::EF_NONE);
  down_swipe_gestures_[4] =
      BindKeyEventWithFlags(VKEY_BROWSER_REFRESH, ui::EF_NONE);
}

}  // namespace ui
