// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_TOUCH_EXPLORATION_CONTROLLER_H_
#define UI_CHROMEOS_TOUCH_EXPLORATION_CONTROLLER_H_

#include "base/timer/timer.h"
#include "base/values.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/events/event.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/gesture_detection/gesture_detector.h"
#include "ui/gfx/geometry/point.h"

namespace aura {
class Window;
}

namespace ui {

class Event;
class EventHandler;
class TouchEvent;

// TouchExplorationController is used in tandem with "Spoken Feedback" to
// make the touch UI accessible.
//
// ** Short version **
//
// At a high-level, single-finger events are used for accessibility -
// exploring the screen gets turned into mouse moves (which can then be
// spoken by an accessibility service running), a double-tap simulates a
// click, and gestures can be used to send high-level accessibility commands.
// When two or more fingers are pressed initially, from then on the events
// are passed through, but with the initial finger removed - so if you swipe
// down with two fingers, the running app will see a one-finger swipe.
//
// ** Long version **
//
// Here are the details of the implementation:
//
// When the first touch is pressed, a 300 ms grace period timer starts.
//
// If the user keeps their finger down for more than 300 ms and doesn't
// perform a supported accessibility gesture in that time (e.g. swipe right),
// they enter touch exploration mode, and all movements are translated into
// synthesized mouse move events.
//
// Also, if the user moves their single finger outside a certain slop region
// (without performing a gesture), they enter touch exploration mode earlier
// than 300 ms.
//
// If the user taps and releases their finger, after 300 ms from the initial
// touch, a single mouse move is fired.
//
// If the user double-taps, the second tap is passed through, allowing the
// user to click - however, the double-tap location is changed to the location
// of the last successful touch exploration - that allows the user to explore
// anywhere on the screen, hear its description, then double-tap anywhere
// to activate it.
//
// If the user enters touch exploration mode, they can click without lifting
// their touch exploration finger by tapping anywhere else on the screen with
// a second finger, while the touch exploration finger is still pressed.
//
// If the user adds a second finger during the grace period, they enter
// passthrough mode. In this mode, the first finger is ignored but all
// additional touch events are mostly passed through unmodified. So a
// two-finger scroll gets passed through as a one-finger scroll. However,
// once in passthrough mode, if one finger is released, the remaining fingers
// continue to pass through events, allowing the user to start a scroll
// with two fingers but finish it with one. Sometimes this requires rewriting
// the touch ids.
//
// Once either touch exploration or passthrough mode has been activated,
// it remains in that mode until all fingers have been released.
//
// The caller is expected to retain ownership of instances of this class and
// destroy them before |root_window| is destroyed.
class UI_CHROMEOS_EXPORT TouchExplorationController :
    public ui::EventRewriter {
 public:
  explicit TouchExplorationController(aura::Window* root_window);
  virtual ~TouchExplorationController();

  void CallTapTimerNowForTesting();
  void SetEventHandlerForTesting(ui::EventHandler* event_handler_for_testing);
  bool IsInNoFingersDownStateForTesting() const;

 private:
  // Overridden from ui::EventRewriter
  virtual ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      scoped_ptr<ui::Event>* rewritten_event) OVERRIDE;
  virtual ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event, scoped_ptr<ui::Event>* new_event) OVERRIDE;

  // Event handlers based on the current state - see State, below.
  ui::EventRewriteStatus InNoFingersDown(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InSingleTapPressed(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InSingleTapReleased(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InDoubleTapPressed(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InTouchExploration(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InPassthroughMinusOne(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InTouchExploreSecondPress(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  // This timer is started every time we get the first press event, and
  // it fires after the double-click timeout elapses (300 ms by default).
  // If the user taps and releases within 300 ms and doesn't press again,
  // we treat that as a single mouse move (touch exploration) event.
  void OnTapTimerFired();

  // Dispatch a new event outside of the event rewriting flow.
  void DispatchEvent(ui::Event* event);

  scoped_ptr<ui::Event> CreateMouseMoveEvent(const gfx::PointF& location,
                                             int flags);

  void EnterTouchToMouseMode();

  // Set the state to NO_FINGERS_DOWN and reset any other fields to their
  // default value.
  void ResetToNoFingersDown();

  enum State {
    // No fingers are down and no events are pending.
    NO_FINGERS_DOWN,

    // A single finger is down, but we're not yet sure if this is going
    // to be touch exploration or something else.
    SINGLE_TAP_PRESSED,

    // The user pressed and released a single finger - a tap - but we have
    // to wait until the end of the grace period to allow the user to tap the
    // second time. If the second tap doesn't occurs within the grace period,
    // we dispatch a mouse move at the location of the first tap.
    SINGLE_TAP_RELEASED,

    // The user tapped once, and before the grace period expired, pressed
    // one finger down to begin a double-tap, but has not released it yet.
    DOUBLE_TAP_PRESSED,

    // We're in touch exploration mode. Anything other than the first finger
    // is ignored, and movements of the first finger are rewritten as mouse
    // move events. This mode is entered if a single finger is pressed and
    // after the grace period the user hasn't added a second finger or
    // moved the finger outside of the slop region. We'll stay in this
    // mode until all fingers are lifted.
    TOUCH_EXPLORATION,

    // The user placed two or more fingers down within the grace period.
    // We're now in passthrough mode until all fingers are lifted. Initially
    // the first finger is ignored and other fingers are passed through
    // as-is. If a finger other than the initial one is the first to be
    // released, we rewrite the first finger with the touch id of the finger
    // that was released, from now on. The motivation for this is that if
    // the user starts a scroll with 2 fingers, they can release either one
    // and continue the scrolling.
    PASSTHROUGH_MINUS_ONE,

    // The user was in touch exploration, but has placed down another finger.
    // If the user releases the second finger, a touch press and release
    // will go through at the last touch explore location. If the user
    // releases the touch explore finger, the other finger will continue with
    // touch explore. Any fingers pressed past the first two are ignored.
    TOUCH_EXPLORE_SECOND_PRESS,
  };

  void VlogState(const char* function_name);

  void VlogEvent(const ui::TouchEvent& event, const char* function_name);

  // Gets enum name from integer value.
  const char* EnumStateToString(State state);

  std::string EnumEventTypeToString(ui::EventType type);

  aura::Window* root_window_;

  // A set of touch ids for fingers currently touching the screen.
  std::vector<int> current_touch_ids_;

  // Map of touch ids to their last known location.
  std::map<int, gfx::PointF> touch_locations_;

  // The touch id that any events on the initial finger should be rewritten
  // as in passthrough-minus-one mode. If kTouchIdUnassigned, events on the
  // initial finger are discarded. If kTouchIdNone, the initial finger
  // has been released and no more rewriting will be done.
  int initial_touch_id_passthrough_mapping_;

  // The current state.
  State state_;

  // A copy of the event from the initial touch press.
  scoped_ptr<ui::TouchEvent> initial_press_;

  // The last synthesized mouse move event. When the user double-taps,
  // we send the passed-through tap to the location of this event.
  scoped_ptr<ui::TouchEvent> last_touch_exploration_;

  // A timer to fire the mouse move event after the double-tap delay.
  base::OneShotTimer<TouchExplorationController> tap_timer_;

  // For testing only, an event handler to use for generated events
  // outside of the normal event rewriting flow.
  ui::EventHandler* event_handler_for_testing_;

  // A default gesture detector config, so we can share the same
  // timeout and pixel slop constants.
  ui::GestureDetector::Config gesture_detector_config_;

  // The previous state entered.
  State prev_state_;

  // A copy of the previous event passed.
  scoped_ptr<ui::TouchEvent> prev_event_;

  DISALLOW_COPY_AND_ASSIGN(TouchExplorationController);
};

}  // namespace ui

#endif  // UI_CHROMEOS_TOUCH_EXPLORATION_CONTROLLER_H_
