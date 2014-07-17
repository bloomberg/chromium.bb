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
#include "ui/events/gestures/gesture_provider_aura.h"
#include "ui/gfx/geometry/point.h"

namespace aura {
class Window;
}

namespace ui {

class Event;
class EventHandler;
class GestureEvent;
class GestureProviderAura;
class TouchEvent;

// TouchExplorationController is used in tandem with "Spoken Feedback" to
// make the touch UI accessible. Gestures are mapped to accessiblity key
// shortcuts.
//
// ** Short version **
//
// At a high-level, single-finger events are used for accessibility -
// exploring the screen gets turned into mouse moves (which can then be
// spoken by an accessibility service running), a single tap while the user
// is in touch exploration or a double-tap simulates a click, and gestures
// can be used to send high-level accessibility commands. For example, a swipe
// right would correspond to the keyboard short cut shift+search+right.
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
// While in touch exploration mode, the user can perform a single tap
// if the user releases their finger and taps before 300 ms passes.
// This will result in a click on the last successful touch exploration
// location. This allows the user to perform a single tap
// anywhere to activate it.
//
// The user can perform swipe gestures in one of the four cardinal directions
// which will be interpreted and used to control the UI. The gesture will only
// be registered if the finger moves outside the slop and completed within the
// grace period. If additional fingers are added during the grace period, the
// state changes to passthrough. If the gesture fails to be completed within the
// grace period, the state changes to touch exploration mode. Once the state has
// changed, any gestures made during the grace period are discarded.
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
// two to one finger passthrough mode. In this mode, the first finger is
// ignored and the user can scroll or drag with the second finger. If either
// finger is released, nothing happens until all fingers are up. If the user
// adds a third finger while in two to one finger mode, all fingers and touch
// events are passed through from then on.
//
// Once touch exploration mode has been activated,
// it remains in that mode until all fingers have been released.
//
// The caller is expected to retain ownership of instances of this class and
// destroy them before |root_window| is destroyed.
class UI_CHROMEOS_EXPORT TouchExplorationController
    : public ui::EventRewriter,
      public ui::GestureProviderAuraClient {
 public:
  explicit TouchExplorationController(aura::Window* root_window);
  virtual ~TouchExplorationController();

  void CallTapTimerNowForTesting();
  void CallTapTimerNowIfRunningForTesting();
  void SetEventHandlerForTesting(ui::EventHandler* event_handler_for_testing);
  bool IsInNoFingersDownStateForTesting() const;
  bool IsInGestureInProgressStateForTesting() const;
  // VLOGs should be suppressed in tests that generate a lot of logs,
  // for example permutations of nine touch events.
  void SuppressVLOGsForTesting(bool suppress);

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
  ui::EventRewriteStatus InSingleTapOrTouchExploreReleased(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InDoubleTapPressed(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InTouchExploration(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InTwoToOneFinger(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InPassthrough(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InGestureInProgress(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InTouchExploreSecondPress(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InWaitForRelease(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);

  // This timer is started every time we get the first press event, and
  // it fires after the double-click timeout elapses (300 ms by default).
  // If the user taps and releases within 300 ms and doesn't press again,
  // we treat that as a single mouse move (touch exploration) event.
  void OnTapTimerFired();

  // Dispatch a new event outside of the event rewriting flow.
  void DispatchEvent(ui::Event* event);

  // Overridden from GestureProviderAuraClient.
  //
  // The gesture provider keeps track of all the touch events after
  // the user moves fast enough to trigger a gesture. After the user
  // completes their gesture, this method will decide what keyboard
  // input their gesture corresponded to.
  virtual void OnGestureEvent(ui::GestureEvent* gesture) OVERRIDE;

  // Process the gesture events that have been created.
  void ProcessGestureEvents();

  void OnSwipeEvent(ui::GestureEvent* swipe_gesture);

  // Dispatches the keyboard short cut Shift+Search+<arrow key>
  // outside the event rewritting flow.
  void DispatchShiftSearchKeyEvent(const ui::KeyboardCode direction);

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

    // The user was in touch explore mode and released the finger.
    // If another touch press occurs within the grace period, a single
    // tap click occurs. This state differs from SINGLE_TAP_RELEASED
    // In that if a second tap doesn't occur within the grace period,
    // there is no mouse move dispatched.
    TOUCH_EXPLORE_RELEASED,

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

    // If the user moves their finger faster than the threshold velocity after a
    // single tap, the touch events that follow will be translated into gesture
    // events. If the user successfully completes a gesture within the grace
    // period, the gesture will be interpreted and used to control the UI via
    // discrete actions - currently by synthesizing key events corresponding to
    // each gesture Otherwise, the collected gestures are discarded and the
    // state changes to touch_exploration.
    GESTURE_IN_PROGRESS,

    // The user was in touch exploration, but has placed down another finger.
    // If the user releases the second finger, a touch press and release
    // will go through at the last touch explore location. If the user
    // releases the touch explore finger, the other finger will continue with
    // touch explore. Any fingers pressed past the first two are ignored.
    TOUCH_EXPLORE_SECOND_PRESS,

    // The user placed two fingers down within the grace period.
    // We're now in two to one finger mode until one of the fingers is
    // lifted. The first finger is ignored and the events for the second
    // finger are passed throuugh. If either finger is released, nothing
    // happens until all fingers are up.
    TWO_TO_ONE_FINGER,

    // If the user is in TWO_TO_ONE_FINGER with two fingers down and presses
    // a third finger, every finger and touch event is passed through until
    // all fingers are released.
    PASSTHROUGH,

    // If the user lifted a finger in TWO_TO_ONE_FINGER, they must release
    // all fingers before completing any more actions. This state is
    // generally useful for developing new features, because it creates a
    // simple way to handle a dead end in user flow.
    WAIT_FOR_RELEASE,
  };

  void VlogState(const char* function_name);

  void VlogEvent(const ui::TouchEvent& event, const char* function_name);

  // Gets enum name from integer value.
  const char* EnumStateToString(State state);

  aura::Window* root_window_;

  // A set of touch ids for fingers currently touching the screen.
  std::vector<int> current_touch_ids_;

  // Map of touch ids to their last known location.
  std::map<int, gfx::PointF> touch_locations_;

  // The current state.
  State state_;

  // A copy of the event from the initial touch press.
  scoped_ptr<ui::TouchEvent> initial_press_;

  // Stores the most recent event from a finger that is currently not
  // sending events through, but might in the future (e.g. TwoToOneFinger
  // to Passthrough state).
  scoped_ptr<ui::TouchEvent> last_unused_finger_event_;

  // The last synthesized mouse move event. When the user double-taps,
  // we send the passed-through tap to the location of this event.
  scoped_ptr<ui::TouchEvent> last_touch_exploration_;

  // The last event from the finger that is being passed through in
  // TWO_TO_ONE_FINGER. When the user lifts a finger during two to one,
  // the location and id of the touch release is from here.
  scoped_ptr<ui::TouchEvent> last_two_to_one_;

  // A timer to fire the mouse move event after the double-tap delay.
  base::OneShotTimer<TouchExplorationController> tap_timer_;

  // For testing only, an event handler to use for generated events
  // outside of the normal event rewriting flow.
  ui::EventHandler* event_handler_for_testing_;

  // A default gesture detector config, so we can share the same
  // timeout and pixel slop constants.
  ui::GestureDetector::Config gesture_detector_config_;

  // Gesture Handler to interpret the touch events.
  ui::GestureProviderAura gesture_provider_;

  // The previous state entered.
  State prev_state_;

  // A copy of the previous event passed.
  scoped_ptr<ui::TouchEvent> prev_event_;

  // This toggles whether VLOGS are turned on or not.
  bool VLOG_on_;

  DISALLOW_COPY_AND_ASSIGN(TouchExplorationController);
};

}  // namespace ui

#endif  // UI_CHROMEOS_TOUCH_EXPLORATION_CONTROLLER_H_
