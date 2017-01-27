// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebTouchEvent_h
#define WebTouchEvent_h

#include "WebInputEvent.h"

namespace blink {

// See WebInputEvent.h for details why this pack is here.
#pragma pack(push, 4)

// WebTouchEvent --------------------------------------------------------------

// TODO(e_hakkinen): Replace with WebPointerEvent. crbug.com/508283
class WebTouchEvent : public WebInputEvent {
 public:
  // Maximum number of simultaneous touches supported on
  // Ash/Aura.
  enum { kTouchesLengthCap = 16 };

  unsigned touchesLength;
  // List of all touches, regardless of state.
  WebTouchPoint touches[kTouchesLengthCap];

  // Whether the event is blocking, non-blocking, all event
  // listeners were passive or was forced to be non-blocking.
  DispatchType dispatchType;

  // For a single touch, this is true after the touch-point has moved beyond
  // the platform slop region. For a multitouch, this is true after any
  // touch-point has moved (by whatever amount).
  bool movedBeyondSlopRegion;

  // Whether this touch event is a touchstart or a first touchmove event per
  // scroll.
  bool touchStartOrFirstTouchMove;

  // A unique identifier for the touch event. Valid ids start at one and
  // increase monotonically. Zero means an unknown id.
  uint32_t uniqueTouchEventId;

  WebTouchEvent()
      : WebInputEvent(sizeof(WebTouchEvent)), dispatchType(Blocking) {}

  WebTouchEvent(Type type, int modifiers, double timeStampSeconds)
      : WebInputEvent(sizeof(WebTouchEvent), type, modifiers, timeStampSeconds),
        dispatchType(Blocking) {}

#if INSIDE_BLINK

  // Sets any scaled values to be their computed values and sets |frameScale|
  // back to 1 and |translateX|, |translateY| back to 0.
  BLINK_PLATFORM_EXPORT WebTouchEvent flattenTransform() const;

  // Return a scaled WebTouchPoint in root frame coordinates.
  BLINK_PLATFORM_EXPORT WebTouchPoint
  touchPointInRootFrame(unsigned touchPoint) const;

  bool isCancelable() const { return dispatchType == Blocking; }
#endif
};

#pragma pack(pop)

}  // namespace blink

#endif  // WebTouchEvent_h
