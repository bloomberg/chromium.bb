// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_GESTURES_GESTURE_RECOGNIZER_H_
#define UI_AURA_GESTURES_GESTURE_RECOGNIZER_H_
#pragma once

#include <map>
#include <vector>

#include "base/memory/linked_ptr.h"
#include "ui/aura/aura_export.h"
#include "ui/base/events.h"
#include "ui/gfx/point.h"

namespace aura {
class TouchEvent;
class GestureEvent;

// A GestureRecognizer recognizes gestures from touch sequences.
class AURA_EXPORT GestureRecognizer {
 public:
  // Gesture state.
  enum GestureState {
    GS_NO_GESTURE,
    GS_PENDING_SYNTHETIC_CLICK,
    GS_SCROLL,
  };

  // ui::EventType is mapped to TouchState so it can fit into 3 bits of
  // Signature.
  enum TouchState {
    TS_RELEASED,
    TS_PRESSED,
    TS_MOVED,
    TS_STATIONARY,
    TS_CANCELLED,
    TS_UNKNOWN,
  };

  // List of GestureEvent*.
  typedef std::vector<linked_ptr<GestureEvent> > Gestures;

  GestureRecognizer();
  virtual ~GestureRecognizer();

  // Invoked for each touch event that could contribute to the current gesture.
  // Returns list of  zero or more GestureEvents identified after processing
  // TouchEvent.
  // Caller would be responsible for freeing up Gestures.
  virtual Gestures* ProcessTouchEventForGesture(const TouchEvent& event,
                                                ui::TouchStatus status);

  // Clears the GestureRecognizer to its initial state.
  virtual void Reset();

  // Accessor function.
  GestureState GetState() const { return state_; }

 private:
  // Gesture signature types for different values of combination (GestureState,
  // touch_id, ui::EventType, touch_handled), see GestureRecognizer::Signature()
  // for more info.
  //
  // Note: New addition of types should be placed as per their Signature value.
  enum GestureSignatureType {
    // For input combination (GS_NO_GESTURE, 0, ui::ET_TOUCH_PRESSED, false).
    GST_NO_GESTURE_FIRST_PRESSED = 0x00000003,

    // (GS_PENDING_SYNTHETIC_CLICK, 0, ui::ET_TOUCH_RELEASED, false).
    GST_PENDING_SYNTHETIC_CLICK_FIRST_RELEASED = 0x00020001,

    // (GS_PENDING_SYNTHETIC_CLICK, 0, ui::ET_TOUCH_MOVED, false).
    GST_PENDING_SYNTHETIC_CLICK_FIRST_MOVED = 0x00020005,

    // (GS_PENDING_SYNTHETIC_CLICK, 0, ui::ET_TOUCH_STATIONARY, false).
    GST_PENDING_SYNTHETIC_CLICK_FIRST_STATIONARY = 0x00020007,

    // (GS_PENDING_SYNTHETIC_CLICK, 0, ui::ET_TOUCH_CANCELLED, false).
    GST_PENDING_SYNTHETIC_CLICK_FIRST_CANCELLED = 0x00020009,

    // (GS_SCROLL, 0, ui::ET_TOUCH_RELEASED, false).
    GST_SCROLL_FIRST_RELEASED = 0x00040001,

    // (GS_SCROLL, 0, ui::ET_TOUCH_MOVED, false).
    GST_SCROLL_FIRST_MOVED = 0x00040005,

    // (GS_SCROLL, 0, ui::ET_TOUCH_CANCELLED, false).
    GST_SCROLL_FIRST_CANCELLED = 0x00040009,
  };

  // Builds a signature. Signatures are assembled by joining together
  // multiple bits.
  // 1 LSB bit so that the computed signature is always greater than 0
  // 3 bits for the |type|.
  // 1 bit for |touch_handled|
  // 12 bits for |touch_id|
  // 15 bits for the |gesture_state|.
  static unsigned int Signature(GestureState state,
                                unsigned int touch_id, ui::EventType type,
                                bool touch_handled);

  // Various statistical functions to manipulate gestures.
  bool IsInClickTimeWindow();
  bool IsInSecondClickTimeWindow();
  bool IsInsideManhattanSquare(const TouchEvent& event);
  bool IsSecondClickInsideManhattanSquare(const TouchEvent& event);
  bool IsOverMinFlickSpeed();

  // Functions to be called to add GestureEvents, after succesful recognition.
  void AppendTapDownGestureEvent(const TouchEvent& event, Gestures* gestures);
  void AppendClickGestureEvent(const TouchEvent& event, Gestures* gestures);
  void AppendDoubleClickGestureEvent(const TouchEvent& event,
                                     Gestures* gestures);
  void AppendScrollGestureBegin(const TouchEvent& event, Gestures* gestures);
  void AppendScrollGestureEnd(const TouchEvent& event,
                               Gestures* gestures,
                               float x_velocity, float y_velocity);
  void AppendScrollGestureUpdate(const TouchEvent& event, Gestures* gestures);

  void UpdateValues(const TouchEvent& event);
  void SetState(const GestureState state ) { state_ = state; }

  // Various GestureTransitionFunctions for a signature.
  // There is, 1:many mapping from GestureTransitionFunction to Signature
  // But a Signature have only one GestureTransitionFunction.
  bool Click(const TouchEvent& event, Gestures* gestures);
  bool InClickOrScroll(const TouchEvent& event, Gestures* gestures);
  bool InScroll(const TouchEvent& event, Gestures* gestures);
  bool NoGesture(const TouchEvent& event, Gestures* gestures);
  bool TouchDown(const TouchEvent& event, Gestures* gestures);
  bool ScrollEnd(const TouchEvent& event, Gestures* gestures);

  // Location of first touch event in a touch sequence.
  gfx::Point first_touch_position_;

  // Time of first touch event in a touch sequence.
  double first_touch_time_;

  // Current state of gesture recognizer.
  GestureState state_;

  // Time of current touch event in a touch sequence.
  double last_touch_time_;

  // Time of click gesture.
  double last_click_time_;

  // Location of click gesture.
  gfx::Point last_click_position_;

  // Location of current touch event in a touch sequence.
  gfx::Point last_touch_position_;

  // Velocity in x and y direction.
  float x_velocity_;
  float y_velocity_;

  // ui::EventFlags.
  int flags_;

  DISALLOW_COPY_AND_ASSIGN(GestureRecognizer);
};

}  // namespace aura

#endif  // UI_AURA_GESTURES_GESTURE_RECOGNIZER_H_
