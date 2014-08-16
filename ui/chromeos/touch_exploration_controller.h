// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_TOUCH_EXPLORATION_CONTROLLER_H_
#define UI_CHROMEOS_TOUCH_EXPLORATION_CONTROLLER_H_

#include "base/time/tick_clock.h"
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

// A delegate to handle commands in response to detected accessibility gesture
// events.
class TouchExplorationControllerDelegate {
 public:
  virtual ~TouchExplorationControllerDelegate() {}

  // Takes an int from 0.0 to 100.0 that indicates the percent the volume
  // should be set to.
  virtual void SetOutputLevel(int volume) = 0;

  // Silences spoken feedback.
  virtual void SilenceSpokenFeedback() = 0;

  // This function should be called when the volume adjust earcon should be
  // played
  virtual void PlayVolumeAdjustEarcon() = 0;

  // This function should be called when the passthrough earcon should be
  // played.
  virtual void PlayPassthroughEarcon() = 0;

  // This function should be called when the exit screen earcon should be
  // played.
  virtual void PlayExitScreenEarcon() = 0;

  // This function should be called when the enter screen earcon should be
  // played.
  virtual void PlayEnterScreenEarcon() = 0;
};

// TouchExplorationController is used in tandem with "Spoken Feedback" to
// make the touch UI accessible. Gestures performed in the middle of the screen
// are mapped to accessibility key shortcuts while gestures performed on the
// edge of the screen can change settings.
//
// ** Short version **
//
// At a high-level, single-finger events are used for accessibility -
// exploring the screen gets turned into mouse moves (which can then be
// spoken by an accessibility service running), a single tap while the user
// is in touch exploration or a double-tap simulates a click, and gestures
// can be used to send high-level accessibility commands. For example, a swipe
// right would correspond to the keyboard short cut shift+search+right.
// Swipes with up to four fingers are also mapped to commands. Slide
// gestures performed on the edge of the screen can change settings
// continuously. For example, sliding a finger along the right side of the
// screen will change the volume. When a user double taps and holds with one
// finger, the finger is passed through as if accessibility was turned off. If
// the user taps the screen with two fingers, the user can silence spoken
// feedback if it is playing.
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
// which will be interpreted and used to control the UI. All gestures will only
// be registered if the fingers move outside the slop, and all fingers will only
// be registered if they are completed within the grace period. If a single
// finger gesture fails to be completed within the grace period, the state
// changes to touch exploration mode. If a multi finger gesture fails to be
// completed within the grace period, the user must lift all fingers before
// completing any more actions.
//
// If the user double-taps, the second tap is passed through, allowing the
// user to click - however, the double-tap location is changed to the location
// of the last successful touch exploration - that allows the user to explore
// anywhere on the screen, hear its description, then double-tap anywhere
// to activate it.
//
// If the user double taps and holds, any event from that finger is passed
// through. These events are passed through with an offset such that the first
// touch is offset to be at the location of the last touch exploration
// location, and every following event is offset by the same amount.
//
// If any other fingers are added or removed, they are ignored. Once the
// passthrough finger is released, passthrough stops and the user is reset
// to no fingers down state.
//
// If the user enters touch exploration mode, they can click without lifting
// their touch exploration finger by tapping anywhere else on the screen with
// a second finger, while the touch exploration finger is still pressed.
//
// Once touch exploration mode has been activated, it remains in that mode until
// all fingers have been released.
//
// If the user places a finger on the edge of the screen and moves their finger
// past slop, a slide gesture is performed. The user can then slide one finger
// along an edge of the screen and continuously control a setting. Once the user
// enters this state, the boundaries that define an edge expand so that the user
// can now adjust the setting within a slightly bigger width along the screen.
// If the user exits this area without lifting their finger, they will not be
// able to perform any actions, however if they keep their finger down and
// return to the "hot edge," then they can still adjust the setting. In order to
// perform other touch accessibility movements, the user must lift their finger.
// If additional fingers are added while in this state, the user will transition
// to passthrough.
//
// Currently, only the right edge is mapped to control the volume. Volume
// control along the edge of the screen is directly proportional to where the
// user's finger is located on the screen. The top right corner of the screen
// automatically sets the volume to 100% and the bottome right corner of the
// screen automatically sets the volume to 0% once the user has moved past slop.
//
// If the user taps the screen with two fingers and lifts both fingers before
// the grace period has passed, spoken feedback is silenced.
//
// The user can also enter passthrough by placing a finger on one of the bottom
// corners of the screen until an earcon sounds. After the earcon sounds, the
// user is in passthrough so all subsequent fingers placed on the screen will be
// passed through. Once the finger in the corner has been released, the state
// will switch to wait for no fingers.
//
// The caller is expected to retain ownership of instances of this class and
// destroy them before |root_window| is destroyed.
class UI_CHROMEOS_EXPORT TouchExplorationController
    : public ui::EventRewriter,
      public ui::GestureProviderAuraClient {
 public:
  explicit TouchExplorationController(
      aura::Window* root_window,
      ui::TouchExplorationControllerDelegate* delegate);
  virtual ~TouchExplorationController();

 private:
  friend class TouchExplorationControllerTestApi;

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
  ui::EventRewriteStatus InDoubleTapPending(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InTouchReleasePending(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InTouchExploration(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InCornerPassthrough(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InOneFingerPassthrough(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InGestureInProgress(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InTouchExploreSecondPress(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InWaitForNoFingers(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InSlideGesture(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);
  ui::EventRewriteStatus InTwoFingerTap(
      const ui::TouchEvent& event, scoped_ptr<ui::Event>* rewritten_event);

  // Returns the current time of the tick clock.
  base::TimeDelta Now();

  // This timer is started every time we get the first press event, and
  // it fires after the double-click timeout elapses (300 ms by default).
  // If the user taps and releases within 300 ms and doesn't press again,
  // we treat that as a single mouse move (touch exploration) event.
  void StartTapTimer();
  void OnTapTimerFired();

  // This timer is started every timer we get the first press event and the
  // finger is in the corner of the screen.
  // It fires after the corner passthrough delay elapses. If the
  // user is still in the corner by the time this timer fires, all subsequent
  // fingers added on the screen will be passed through.
  void OnPassthroughTimerFired();

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

  void SideSlideControl(ui::GestureEvent* gesture);

  // Dispatches the keyboard short cut Shift+Search+<arrow key>
  // outside the event rewritting flow.
  void DispatchShiftSearchKeyEvent(const ui::KeyboardCode third_key);

  // Binds DispatchShiftSearchKeyEvent to a specific third key.
  base::Closure BindShiftSearchKeyEvent(const ui::KeyboardCode third_key);

  // Dispatches a single key with the given flags.
  void DispatchKeyWithFlags(const ui::KeyboardCode key, int flags);

  // Binds DispatchKeyWithFlags to a specific key and flags.
  base::Closure BindKeyEventWithFlags(const ui::KeyboardCode key, int flags);

  scoped_ptr<ui::Event> CreateMouseMoveEvent(const gfx::PointF& location,
                                             int flags);

  void EnterTouchToMouseMode();

  void PlaySoundForTimer();

  // Some constants used in touch_exploration_controller:

  // Within this many dips of the screen edge, the release event generated will
  // reset the state to NoFingersDown.
  const float kLeavingScreenEdge = 6;

  // Swipe/scroll gestures within these bounds (in DIPs) will change preset
  // settings.
  const float kMaxDistanceFromEdge = 75;

  // After a slide gesture has been triggered, if the finger is still within
  // these bounds (in DIPs), the preset settings will still change.
  const float kSlopDistanceFromEdge = kMaxDistanceFromEdge + 40;

  // The split tap slop  is a bit more generous since keeping two
  // fingers in place is a bit harder.
  const float GetSplitTapTouchSlop();

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
    // in that if a second tap doesn't occur within the grace period,
    // there is no mouse move dispatched.
    TOUCH_EXPLORE_RELEASED,

    // The user tapped once, and before the grace period expired, pressed
    // one finger down to begin a double-tap, but has not released it yet.
    // This could become passthrough, so no touch press is dispatched yet.
    DOUBLE_TAP_PENDING,

    // The user was doing touch exploration, started split tap, but lifted the
    // touch exploration finger. Once they remove all fingers, a touch release
    // will go through.
    TOUCH_RELEASE_PENDING,

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
    // releases the touch explore finger, the touch press and release will
    // still go through once the split tap finger is also lifted. If any
    // fingers pressed past the first two, the touch press is cancelled and
    // the user enters the wait state for the fingers to be removed.
    TOUCH_EXPLORE_SECOND_PRESS,

    // After the user double taps and holds with a single finger, all events
    // for that finger are passed through, displaced by an offset. Adding
    // extra fingers has no effect. This state is left when the user removes
    // all fingers.
    ONE_FINGER_PASSTHROUGH,

    // If the user has pressed and held down the left corner past long press,
    // then as long as they are holding the corner, all subsequent fingers
    // registered will be in passthrough.
    CORNER_PASSTHROUGH,

    // If the user added another finger in SINGLE_TAP_PRESSED, or if the user
    // has multiple fingers fingers down in any other state between
    // passthrough, touch exploration, and gestures, they must release
    // all fingers before completing any more actions. This state is
    // generally useful for developing new features, because it creates a
    // simple way to handle a dead end in user flow.
    WAIT_FOR_NO_FINGERS,

    // If the user is within the given bounds from an edge of the screen, not
    // including corners, then the resulting movements will be interpreted as
    // slide gestures.
    SLIDE_GESTURE,

    // If the user taps the screen with two fingers and releases both fingers
    // before the grace period has passed, spoken feedback will be silenced.
    TWO_FINGER_TAP,
  };

  enum ScreenLocation {
    // Hot "edges" of the screen are each represented by a respective bit.
    NO_EDGE = 0,
    RIGHT_EDGE = 1 << 0,
    TOP_EDGE = 1 << 1,
    LEFT_EDGE = 1 << 2,
    BOTTOM_EDGE = 1 << 3,
    BOTTOM_LEFT_CORNER = LEFT_EDGE | BOTTOM_EDGE,
    BOTTOM_RIGHT_CORNER = RIGHT_EDGE | BOTTOM_EDGE,
  };

  // Given a point, if it is within the given bounds of an edge, returns the
  // edge. If it is within the given bounds of two edges, returns an int with
  // both bits that represent the respective edges turned on. Otherwise returns
  // SCREEN_CENTER.
  int FindEdgesWithinBounds(gfx::Point point, float bounds);

  // Set the state and modifies any variables related to the state change.
  // (e.g. resetting the gesture provider).
  void SetState(State new_state, const char* function_name);

  void VlogState(const char* function_name);

  void VlogEvent(const ui::TouchEvent& event, const char* function_name);

  // Gets enum name from integer value.
  const char* EnumStateToString(State state);

  // Maps each single/multi finger swipe to the function that dispatches
  // the corresponding key events.
  void InitializeSwipeGestureMaps();

  aura::Window* root_window_;

  // Handles volume control. Not owned.
  ui::TouchExplorationControllerDelegate* delegate_;

  // A set of touch ids for fingers currently touching the screen.
  std::vector<int> current_touch_ids_;

  // Map of touch ids to their last known location.
  std::map<int, gfx::PointF> touch_locations_;

  // The current state.
  State state_;

  // A copy of the event from the initial touch press.
  scoped_ptr<ui::TouchEvent> initial_press_;

  // Map of touch ids to where its initial press occurred relative to the
  // screen.
  std::map<int, gfx::Point> initial_presses_;

  // In one finger passthrough, the touch is displaced relative to the
  // last touch exploration location.
  gfx::Vector2d passthrough_offset_;

  // Stores the most recent event from a finger that is currently not
  // sending events through, but might in the future (e.g. before a finger
  // enters double-tap-hold passthrough, we need to update its location.)
  scoped_ptr<ui::TouchEvent> last_unused_finger_event_;

  // The last synthesized mouse move event. When the user double-taps,
  // we send the passed-through tap to the location of this event.
  scoped_ptr<ui::TouchEvent> last_touch_exploration_;

  // A timer that fires after the double-tap delay.
  base::OneShotTimer<TouchExplorationController> tap_timer_;

  // A timer that fires to enter passthrough.
  base::OneShotTimer<TouchExplorationController> passthrough_timer_;

  // A timer to fire an indicating sound when sliding to change volume.
  base::RepeatingTimer<TouchExplorationController> sound_timer_;

  // A default gesture detector config, so we can share the same
  // timeout and pixel slop constants.
  ui::GestureDetector::Config gesture_detector_config_;

  // Gesture Handler to interpret the touch events.
  scoped_ptr<ui::GestureProviderAura> gesture_provider_;

  // The previous state entered.
  State prev_state_;

  // A copy of the previous event passed.
  scoped_ptr<ui::TouchEvent> prev_event_;

  // This toggles whether VLOGS are turned on or not.
  bool VLOG_on_;

  // When touch_exploration_controller gets time relative to real time during
  // testing, this clock is set to the simulated clock and used.
  base::TickClock* tick_clock_;

  // Maps the number of fingers in a swipe to the resulting functions that
  // dispatch key events.
  std::map<int, base::Closure> left_swipe_gestures_;
  std::map<int, base::Closure> right_swipe_gestures_;
  std::map<int, base::Closure> up_swipe_gestures_;
  std::map<int, base::Closure> down_swipe_gestures_;

  DISALLOW_COPY_AND_ASSIGN(TouchExplorationController);
};

}  // namespace ui

#endif  // UI_CHROMEOS_TOUCH_EXPLORATION_CONTROLLER_H_
