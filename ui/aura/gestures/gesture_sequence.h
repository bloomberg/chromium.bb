// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_GESTURES_GESTURE_SEQUENCE_H_
#define UI_AURA_GESTURES_GESTURE_SEQUENCE_H_
#pragma once

#include "ui/aura/gestures/gesture_point.h"
#include "ui/aura/gestures/gesture_recognizer.h"
#include "ui/base/events.h"

namespace aura {
class TouchEvent;
class GestureEvent;

// A GestureSequence recognizes gestures from touch sequences.
class GestureSequence {
 public:
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

  GestureSequence();
  virtual ~GestureSequence();

  typedef GestureRecognizer::Gestures Gestures;

  // Invoked for each touch event that could contribute to the current gesture.
  // Returns list of  zero or more GestureEvents identified after processing
  // TouchEvent.
  // Caller would be responsible for freeing up Gestures.
  virtual Gestures* ProcessTouchEventForGesture(const TouchEvent& event,
                                                ui::TouchStatus status);

 private:
  // Gesture signature types for different values of combination (GestureState,
  // touch_id, ui::EventType, touch_handled), see GestureSequence::Signature()
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

  void Reset();

  GesturePoint& GesturePointForEvent(const TouchEvent& event);

  // Functions to be called to add GestureEvents, after succesful recognition.

  // Tap gestures.
  void AppendTapDownGestureEvent(const GesturePoint& point, Gestures* gestures);
  void AppendClickGestureEvent(const GesturePoint& point, Gestures* gestures);
  void AppendDoubleClickGestureEvent(const GesturePoint& point,
                                     Gestures* gestures);
  // Scroll gestures.
  void AppendScrollGestureBegin(const GesturePoint& point, Gestures* gestures);
  void AppendScrollGestureEnd(const GesturePoint& point,
                              Gestures* gestures,
                              float x_velocity, float y_velocity);
  void AppendScrollGestureUpdate(const GesturePoint& point, Gestures* gestures);

  void set_state(const GestureState state ) { state_ = state; }

  // Various GestureTransitionFunctions for a signature.
  // There is, 1:many mapping from GestureTransitionFunction to Signature
  // But a Signature have only one GestureTransitionFunction.
  bool Click(const TouchEvent& event,
             const GesturePoint& point,
             Gestures* gestures);
  bool InClickOrScroll(const TouchEvent& event,
                       const GesturePoint& point,
                       Gestures* gestures);
  bool InScroll(const TouchEvent& event,
                const GesturePoint& point,
                Gestures* gestures);
  bool NoGesture(const TouchEvent& event,
                 const GesturePoint& point,
                 Gestures* gestures);
  bool TouchDown(const TouchEvent& event,
                 const GesturePoint& point,
                 Gestures* gestures);
  bool ScrollEnd(const TouchEvent& event,
                 const GesturePoint& point,
                 Gestures* gestures);

  // Current state of gesture recognizer.
  GestureState state_;

  // Location of click gesture.
  gfx::Point last_click_position_;

  // ui::EventFlags.
  int flags_;

  // Maximum points in a single gesture.
  static const int kMaxGesturePoints = 12;

  GesturePoint points_[kMaxGesturePoints];
  int point_count_;

  DISALLOW_COPY_AND_ASSIGN(GestureSequence);
};

}  // namespace aura

#endif  // UI_AURA_GESTURES_GESTURE_SEQUENCE_H_
