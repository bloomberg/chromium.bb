// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/touch_exploration_controller.h"

#include "base/strings/string_number_conversions.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_processor.h"
#include "ui/gfx/geometry/rect.h"

#define VLOG_STATE() if (VLOG_IS_ON(0)) VlogState(__func__)
#define VLOG_EVENT(event) if (VLOG_IS_ON(0)) VlogEvent(event, __func__)

namespace ui {

namespace {

// Delay between adjustment sounds.
const base::TimeDelta kSoundDelay = base::TimeDelta::FromMilliseconds(150);

// In ChromeOS, VKEY_LWIN is synonymous for the search key.
const ui::KeyboardCode kChromeOSSearchKey = ui::VKEY_LWIN;
}  // namespace

TouchExplorationController::TouchExplorationController(
    aura::Window* root_window,
    TouchExplorationControllerDelegate* delegate)
    : root_window_(root_window),
      delegate_(delegate),
      state_(NO_FINGERS_DOWN),
      event_handler_for_testing_(NULL),
      gesture_provider_(this),
      prev_state_(NO_FINGERS_DOWN),
      VLOG_on_(true) {
  CHECK(root_window);
  root_window->GetHost()->GetEventSource()->AddEventRewriter(this);
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

  const ui::EventType type = touch_event.type();
  const gfx::PointF& location = touch_event.location_f();
  const int touch_id = touch_event.touch_id();

  // Always update touch ids and touch locations, so we can use those
  // no matter what state we're in.
  if (type == ui::ET_TOUCH_PRESSED) {
    current_touch_ids_.push_back(touch_id);
    touch_locations_.insert(std::pair<int, gfx::PointF>(touch_id, location));
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    // In order to avoid accidentally double tapping when moving off the edge of
    // the screen, the state will be rewritten to NoFingersDown.
    TouchEvent touch_event = static_cast<const TouchEvent&>(event);
    if (FindEdgesWithinBounds(touch_event.location(), kLeavingScreenEdge) !=
        NO_EDGE) {
      if (current_touch_ids_.size() == 0)
        ResetToNoFingersDown();
    }

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
  }
  VLOG_STATE();
  VLOG_EVENT(touch_event);
  // The rest of the processing depends on what state we're in.
  switch(state_) {
    case NO_FINGERS_DOWN:
      return InNoFingersDown(touch_event, rewritten_event);
    case SINGLE_TAP_PRESSED:
      return InSingleTapPressed(touch_event, rewritten_event);
    case SINGLE_TAP_RELEASED:
    case TOUCH_EXPLORE_RELEASED:
      return InSingleTapOrTouchExploreReleased(touch_event, rewritten_event);
    case DOUBLE_TAP_PRESSED:
      return InDoubleTapPressed(touch_event, rewritten_event);
    case TOUCH_EXPLORATION:
      return InTouchExploration(touch_event, rewritten_event);
    case GESTURE_IN_PROGRESS:
      return InGestureInProgress(touch_event, rewritten_event);
    case TOUCH_EXPLORE_SECOND_PRESS:
      return InTouchExploreSecondPress(touch_event, rewritten_event);
    case TWO_TO_ONE_FINGER:
      return InTwoToOneFinger(touch_event, rewritten_event);
    case PASSTHROUGH:
      return InPassthrough(touch_event, rewritten_event);
    case WAIT_FOR_RELEASE:
      return InWaitForRelease(touch_event, rewritten_event);
    case SLIDE_GESTURE:
      return InSlideGesture(touch_event, rewritten_event);
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
  if (type == ui::ET_TOUCH_PRESSED) {
    initial_press_.reset(new TouchEvent(event));
    last_unused_finger_event_.reset(new TouchEvent(event));
    tap_timer_.Start(FROM_HERE,
                     gesture_detector_config_.double_tap_timeout,
                     this,
                     &TouchExplorationController::OnTapTimerFired);
    gesture_provider_.OnTouchEvent(event);
    gesture_provider_.OnTouchEventAck(false);
    ProcessGestureEvents();
    state_ = SINGLE_TAP_PRESSED;
    VLOG_STATE();
    return ui::EVENT_REWRITE_DISCARD;
  }
  NOTREACHED() << "Unexpected event type received: " << event.name();;
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InSingleTapPressed(
    const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();

  if (type == ui::ET_TOUCH_PRESSED) {
    // Adding a second finger within the timeout period switches to
    // passing through every event from the second finger and none form the
    // first. The event from the first finger is still saved in initial_press_.
    state_ = TWO_TO_ONE_FINGER;
    last_two_to_one_.reset(new TouchEvent(event));
    rewritten_event->reset(new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
                                              event.location(),
                                              event.touch_id(),
                                              event.time_stamp()));
    (*rewritten_event)->set_flags(event.flags());
    return EVENT_REWRITE_REWRITTEN;
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    DCHECK_EQ(0U, current_touch_ids_.size());
    state_ = SINGLE_TAP_RELEASED;
    VLOG_STATE();
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
    VLOG(0) << "\n Delta time: " << delta_time
            << "\n Distance: " << distance
            << "\n Velocity of click: " << velocity
            << "\n Minimum swipe velocity: "
            << gesture_detector_config_.minimum_swipe_velocity;

    // Change to slide gesture if the slide occurred at the right edge.
    int edge = FindEdgesWithinBounds(event.location(), kMaxDistanceFromEdge);
    if (edge & RIGHT_EDGE) {
      state_ = SLIDE_GESTURE;
      VLOG_STATE();
      return InSlideGesture(event, rewritten_event);
    }

    // If the user moves fast enough from the initial touch location, start
    // gesture detection. Otherwise, jump to the touch exploration mode early.
    if (velocity > gesture_detector_config_.minimum_swipe_velocity) {
      state_ = GESTURE_IN_PROGRESS;
      VLOG_STATE();
      return InGestureInProgress(event, rewritten_event);
    }
    EnterTouchToMouseMode();
    state_ = TOUCH_EXPLORATION;
    VLOG_STATE();
    return InTouchExploration(event, rewritten_event);
  }
  NOTREACHED() << "Unexpected event type received: " << event.name();;
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus
TouchExplorationController::InSingleTapOrTouchExploreReleased(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();
  // If there is more than one finger down, then discard to wait until only one
  // finger is or no fingers are down.
  if (current_touch_ids_.size() > 1) {
    state_ = WAIT_FOR_RELEASE;
    return ui::EVENT_REWRITE_DISCARD;
  }
  // If there is no touch exploration yet, discard.
  if (!last_touch_exploration_ || type == ui::ET_TOUCH_RELEASED) {
    if (current_touch_ids_.size() == 0) {
      ResetToNoFingersDown();
    }
    return ui::EVENT_REWRITE_DISCARD;
  }

  if (type == ui::ET_TOUCH_PRESSED) {
    // This is the second tap in a double-tap (or double tap-hold).
    // Rewrite at location of last touch exploration.
    rewritten_event->reset(
        new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
                           last_touch_exploration_->location(),
                           event.touch_id(),
                           event.time_stamp()));
    (*rewritten_event)->set_flags(event.flags());
    state_ = DOUBLE_TAP_PRESSED;
    VLOG_STATE();
    return ui::EVENT_REWRITE_REWRITTEN;
  } else if (type == ui::ET_TOUCH_RELEASED && !last_touch_exploration_) {
    // If the previous press was discarded, we need to also handle its
    // release.
    if (current_touch_ids_.size() == 0) {
      ResetToNoFingersDown();
    }
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_MOVED){
    return ui::EVENT_REWRITE_DISCARD;
  }
  NOTREACHED() << "Unexpected event type received: " << event.name();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InDoubleTapPressed(
    const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();
  if (type == ui::ET_TOUCH_PRESSED) {
    return ui::EVENT_REWRITE_DISCARD;
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    if (current_touch_ids_.size() != 0)
      return EVENT_REWRITE_DISCARD;

    // Rewrite release at location of last touch exploration with the same
    // id as the previous press.
    rewritten_event->reset(
        new ui::TouchEvent(ui::ET_TOUCH_RELEASED,
                           last_touch_exploration_->location(),
                           initial_press_->touch_id(),
                           event.time_stamp()));
    (*rewritten_event)->set_flags(event.flags());
    ResetToNoFingersDown();
    return ui::EVENT_REWRITE_REWRITTEN;
  } else if (type == ui::ET_TOUCH_MOVED) {
    return ui::EVENT_REWRITE_DISCARD;
  }
  NOTREACHED() << "Unexpected event type received: " << event.name();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InTouchExploration(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  const ui::EventType type = event.type();
  if (type == ui::ET_TOUCH_PRESSED) {
    // Handle split-tap.
    initial_press_.reset(new TouchEvent(event));
    if (tap_timer_.IsRunning())
      tap_timer_.Stop();
    rewritten_event->reset(
        new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
                           last_touch_exploration_->location(),
                           event.touch_id(),
                           event.time_stamp()));
    (*rewritten_event)->set_flags(event.flags());
    state_ = TOUCH_EXPLORE_SECOND_PRESS;
    VLOG_STATE();
    return ui::EVENT_REWRITE_REWRITTEN;
  } else if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    initial_press_.reset(new TouchEvent(event));
    tap_timer_.Start(FROM_HERE,
                     gesture_detector_config_.double_tap_timeout,
                     this,
                     &TouchExplorationController::OnTapTimerFired);
    state_ = TOUCH_EXPLORE_RELEASED;
    VLOG_STATE();
  } else if (type != ui::ET_TOUCH_MOVED) {
    NOTREACHED() << "Unexpected event type received: " << event.name();
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
  ui::EventType type = event.type();
  // If additional fingers are added before a swipe gesture has been registered,
  // then the state will no longer be GESTURE_IN_PROGRESS.
  if (type == ui::ET_TOUCH_PRESSED ||
      event.touch_id() != initial_press_->touch_id()) {
    if (tap_timer_.IsRunning())
      tap_timer_.Stop();
    // Discard any pending gestures.
    ignore_result(gesture_provider_.GetAndResetPendingGestures());
    state_ = TWO_TO_ONE_FINGER;
    last_two_to_one_.reset(new TouchEvent(event));
    rewritten_event->reset(new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
                                              event.location(),
                                              event.touch_id(),
                                              event.time_stamp()));
    (*rewritten_event)->set_flags(event.flags());
    return EVENT_REWRITE_REWRITTEN;
  }

  // There should not be more than one finger down.
  DCHECK(current_touch_ids_.size() <= 1);
  if (type == ui::ET_TOUCH_MOVED) {
    gesture_provider_.OnTouchEvent(event);
    gesture_provider_.OnTouchEventAck(false);
  }
  if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    gesture_provider_.OnTouchEvent(event);
    gesture_provider_.OnTouchEventAck(false);
    if (current_touch_ids_.size() == 0)
      ResetToNoFingersDown();
  }

  ProcessGestureEvents();
  return ui::EVENT_REWRITE_DISCARD;
}

ui::EventRewriteStatus TouchExplorationController::InTwoToOneFinger(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  // The user should only ever be in TWO_TO_ONE_FINGER with two fingers down.
  // If the user added or removed a finger, the state is changed.
  ui::EventType type = event.type();
  if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    DCHECK(current_touch_ids_.size() == 1);
    // Stop passing through the second finger and go to the wait state.
    if (current_touch_ids_.size() == 1) {
      rewritten_event->reset(new ui::TouchEvent(ui::ET_TOUCH_RELEASED,
                                                last_two_to_one_->location(),
                                                last_two_to_one_->touch_id(),
                                                event.time_stamp()));
      (*rewritten_event)->set_flags(event.flags());
      state_ = WAIT_FOR_RELEASE;
      return ui::EVENT_REWRITE_REWRITTEN;
    }
  } else if (type == ui::ET_TOUCH_PRESSED) {
    DCHECK(current_touch_ids_.size() == 3);
    // If a third finger is pressed, we are now going into passthrough mode
    // and now need to dispatch the first finger into a press, as well as the
    // recent press.
    if (current_touch_ids_.size() == 3){
      state_ = PASSTHROUGH;
      scoped_ptr<ui::TouchEvent> first_finger_press;
      first_finger_press.reset(
          new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
                             last_unused_finger_event_->location(),
                             last_unused_finger_event_->touch_id(),
                             event.time_stamp()));
      DispatchEvent(first_finger_press.get());
      rewritten_event->reset(new ui::TouchEvent(ui::ET_TOUCH_PRESSED,
                                                event.location(),
                                                event.touch_id(),
                                                event.time_stamp()));
      (*rewritten_event)->set_flags(event.flags());
      return ui::EVENT_REWRITE_REWRITTEN;
    }
  } else if (type == ui::ET_TOUCH_MOVED) {
    DCHECK(current_touch_ids_.size() == 2);
    // The first finger should have no events pass through, but for a proper
    // conversion to passthrough, the press of the initial finger should
    // be updated.
    if (event.touch_id() == last_unused_finger_event_->touch_id()) {
      last_unused_finger_event_.reset(new TouchEvent(event));
      return ui::EVENT_REWRITE_DISCARD;
    }
    if (event.touch_id() == last_two_to_one_->touch_id()) {
      last_two_to_one_.reset(new TouchEvent(event));
      rewritten_event->reset(new ui::TouchEvent(ui::ET_TOUCH_MOVED,
                                                event.location(),
                                                event.touch_id(),
                                                event.time_stamp()));
      (*rewritten_event)->set_flags(event.flags());
      return ui::EVENT_REWRITE_REWRITTEN;
    }
  }
  NOTREACHED() << "Unexpected event type received: " << event.name();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InPassthrough(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  ui::EventType type = event.type();

  if (!(type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED ||
        type == ui::ET_TOUCH_MOVED || type == ui::ET_TOUCH_PRESSED)) {
    NOTREACHED() << "Unexpected event type received: " << event.name();
    return ui::EVENT_REWRITE_CONTINUE;
  }

  rewritten_event->reset(new ui::TouchEvent(
      type, event.location(), event.touch_id(), event.time_stamp()));
  (*rewritten_event)->set_flags(event.flags());

  if (current_touch_ids_.size() == 0) {
    ResetToNoFingersDown();
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
      state_ = DOUBLE_TAP_PRESSED;
      VLOG_STATE();
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
    state_ = TOUCH_EXPLORATION;
    EnterTouchToMouseMode();
    VLOG_STATE();
    return ui::EVENT_REWRITE_REWRITTEN;
  }
  NOTREACHED() << "Unexpected event type received: " << event.name();
  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus TouchExplorationController::InWaitForRelease(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  ui::EventType type = event.type();
  if (!(type == ui::ET_TOUCH_PRESSED || type == ui::ET_TOUCH_MOVED ||
        type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED)) {
    NOTREACHED() << "Unexpected event type received: " << event.name();
    return ui::EVENT_REWRITE_CONTINUE;
  }
  if (current_touch_ids_.size() == 0) {
    state_ = NO_FINGERS_DOWN;
    VLOG_STATE();
    ResetToNoFingersDown();
  }
  return EVENT_REWRITE_DISCARD;
}

void TouchExplorationController::PlaySoundForTimer() {
  delegate_->PlayVolumeAdjustSound();
}

ui::EventRewriteStatus TouchExplorationController::InSlideGesture(
    const ui::TouchEvent& event,
    scoped_ptr<ui::Event>* rewritten_event) {
  // The timer should not fire when sliding.
  if (tap_timer_.IsRunning())
    tap_timer_.Stop();

  ui::EventType type = event.type();
  // If additional fingers are added before a swipe gesture has been registered,
  // then wait until all fingers have been lifted.
  if (type == ui::ET_TOUCH_PRESSED ||
      event.touch_id() != initial_press_->touch_id()) {
    if (sound_timer_.IsRunning())
      sound_timer_.Stop();
    // Discard any pending gestures.
    ignore_result(gesture_provider_.GetAndResetPendingGestures());
    state_ = WAIT_FOR_RELEASE;
    return EVENT_REWRITE_DISCARD;
  }

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
    delegate_->PlayVolumeAdjustSound();
  }

  // There should not be more than one finger down.
  DCHECK(current_touch_ids_.size() <= 1);
  if (type == ui::ET_TOUCH_MOVED) {
    gesture_provider_.OnTouchEvent(event);
    gesture_provider_.OnTouchEventAck(false);
  }
  if (type == ui::ET_TOUCH_RELEASED || type == ui::ET_TOUCH_CANCELLED) {
    gesture_provider_.OnTouchEvent(event);
    gesture_provider_.OnTouchEventAck(false);
    ignore_result(gesture_provider_.GetAndResetPendingGestures());
    if (current_touch_ids_.size() == 0)
      ResetToNoFingersDown();
    return ui::EVENT_REWRITE_DISCARD;
  }

  ProcessGestureEvents();
  return ui::EVENT_REWRITE_DISCARD;
}

void TouchExplorationController::OnTapTimerFired() {
  switch (state_) {
    case SINGLE_TAP_RELEASED:
      ResetToNoFingersDown();
      break;
    case TOUCH_EXPLORE_RELEASED:
      ResetToNoFingersDown();
      last_touch_exploration_.reset(new TouchEvent(*initial_press_));
      return;
    case SINGLE_TAP_PRESSED:
    case GESTURE_IN_PROGRESS:
      // Discard any pending gestures.
      ignore_result(gesture_provider_.GetAndResetPendingGestures());
      EnterTouchToMouseMode();
      state_ = TOUCH_EXPLORATION;
      VLOG_STATE();
      break;
    default:
      return;
  }
  scoped_ptr<ui::Event> mouse_move =
      CreateMouseMoveEvent(initial_press_->location(), initial_press_->flags());
  DispatchEvent(mouse_move.get());
  last_touch_exploration_.reset(new TouchEvent(*initial_press_));
}

void TouchExplorationController::DispatchEvent(ui::Event* event) {
  if (event_handler_for_testing_) {
    event_handler_for_testing_->OnEvent(event);
    return;
  }
  ui::EventDispatchDetails result ALLOW_UNUSED =
      root_window_->GetHost()->dispatcher()->OnEventFromSource(event);
}

void TouchExplorationController::OnGestureEvent(
    ui::GestureEvent* gesture) {
  CHECK(gesture->IsGestureEvent());
  ui::EventType type = gesture->type();
  VLOG(0) << " \n Gesture Triggered: " << gesture->name();
  if (type == ui::ET_GESTURE_SWIPE && state_ != SLIDE_GESTURE) {
    VLOG(0) << "Swipe!";
    ignore_result(gesture_provider_.GetAndResetPendingGestures());
    OnSwipeEvent(gesture);
    return;
  }
}

void TouchExplorationController::ProcessGestureEvents() {
  scoped_ptr<ScopedVector<ui::GestureEvent> > gestures(
      gesture_provider_.GetAndResetPendingGestures());
  if (gestures) {
    for (ScopedVector<GestureEvent>::iterator i = gestures->begin();
         i != gestures->end();
         ++i) {
      if (state_ == SLIDE_GESTURE)
        SideSlideControl(*i);
      else
        OnGestureEvent(*i);
    }
  }
}

void TouchExplorationController::SideSlideControl(ui::GestureEvent* gesture) {
  ui::EventType type = gesture->type();
  if (!gesture->IsScrollGestureEvent())
    return;

  if (type == ET_GESTURE_SCROLL_BEGIN) {
    delegate_->PlayVolumeAdjustSound();
  }

  if (type == ET_GESTURE_SCROLL_END) {
    if (sound_timer_.IsRunning())
      sound_timer_.Stop();
    delegate_->PlayVolumeAdjustSound();
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
  VLOG(0) << "\n Volume = " << volume << "\n Location = " << location.ToString()
          << "\n Bounds = " << root_window_->bounds().right();

  delegate_->SetOutputLevel(int(volume));
}


void TouchExplorationController::OnSwipeEvent(ui::GestureEvent* swipe_gesture) {
  // A swipe gesture contains details for the direction in which the swipe
  // occurred.
  GestureEventDetails event_details = swipe_gesture->details();
  if (event_details.swipe_left()) {
    DispatchShiftSearchKeyEvent(ui::VKEY_LEFT);
    return;
  } else if (event_details.swipe_right()) {
    DispatchShiftSearchKeyEvent(ui::VKEY_RIGHT);
    return;
  } else if (event_details.swipe_up()) {
    DispatchShiftSearchKeyEvent(ui::VKEY_UP);
    return;
  } else if (event_details.swipe_down()) {
    DispatchShiftSearchKeyEvent(ui::VKEY_DOWN);
    return;
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
    const ui::KeyboardCode direction) {
  // In order to activate the shortcut shift+search+<arrow key>
  // three KeyPressed events must be dispatched in succession along
  // with three KeyReleased events.
  ui::KeyEvent shift_down = ui::KeyEvent(
      ui::ET_KEY_PRESSED, ui::VKEY_SHIFT, ui::EF_SHIFT_DOWN, false);
  ui::KeyEvent search_down = ui::KeyEvent(
      ui::ET_KEY_PRESSED, kChromeOSSearchKey, ui::EF_SHIFT_DOWN, false);
  ui::KeyEvent direction_down =
      ui::KeyEvent(ui::ET_KEY_PRESSED, direction, ui::EF_SHIFT_DOWN, false);

  ui::KeyEvent direction_up =
      ui::KeyEvent(ui::ET_KEY_RELEASED, direction, ui::EF_SHIFT_DOWN, false);
  ui::KeyEvent search_up = ui::KeyEvent(
      ui::ET_KEY_RELEASED, kChromeOSSearchKey, ui::EF_SHIFT_DOWN, false);
  ui::KeyEvent shift_up =
      ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_SHIFT, ui::EF_NONE, false);

  DispatchEvent(&shift_down);
  DispatchEvent(&search_down);
  DispatchEvent(&direction_down);
  DispatchEvent(&direction_up);
  DispatchEvent(&search_up);
  DispatchEvent(&shift_up);
}

scoped_ptr<ui::Event> TouchExplorationController::CreateMouseMoveEvent(
    const gfx::PointF& location,
    int flags) {
  return scoped_ptr<ui::Event>(
      new ui::MouseEvent(
          ui::ET_MOUSE_MOVED,
          location,
          location,
          flags | ui::EF_IS_SYNTHESIZED | ui::EF_TOUCH_ACCESSIBILITY,
          0));
}

void TouchExplorationController::EnterTouchToMouseMode() {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root_window_);
  if (cursor_client && !cursor_client->IsMouseEventsEnabled())
    cursor_client->EnableMouseEvents();
  if (cursor_client && cursor_client->IsCursorVisible())
    cursor_client->HideCursor();
}

void TouchExplorationController::ResetToNoFingersDown() {
  ProcessGestureEvents();
  if (sound_timer_.IsRunning())
    sound_timer_.Stop();
  state_ = NO_FINGERS_DOWN;
  VLOG_STATE();
  if (tap_timer_.IsRunning())
    tap_timer_.Stop();
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

  CHECK(touch_event.IsTouchEvent());
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
    case DOUBLE_TAP_PRESSED:
      return "DOUBLE_TAP_PRESSED";
    case TOUCH_EXPLORATION:
      return "TOUCH_EXPLORATION";
    case GESTURE_IN_PROGRESS:
      return "GESTURE_IN_PROGRESS";
    case TOUCH_EXPLORE_SECOND_PRESS:
      return "TOUCH_EXPLORE_SECOND_PRESS";
    case TWO_TO_ONE_FINGER:
      return "TWO_TO_ONE_FINGER";
    case PASSTHROUGH:
      return "PASSTHROUGH";
    case WAIT_FOR_RELEASE:
      return "WAIT_FOR_RELEASE";
    case SLIDE_GESTURE:
      return "SLIDE_GESTURE";
  }
  return "Not a state";
}

}  // namespace ui
