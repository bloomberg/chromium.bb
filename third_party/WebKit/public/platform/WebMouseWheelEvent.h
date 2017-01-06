// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMouseWheelEvent_h
#define WebMouseWheelEvent_h

#include "WebMouseEvent.h"

namespace blink {

// See WebInputEvent.h for details why this pack is here.
#pragma pack(push, 4)

// WebMouseWheelEvent ---------------------------------------------------------

class WebMouseWheelEvent : public WebMouseEvent {
 public:
  enum Phase {
    PhaseNone = 0,
    PhaseBegan = 1 << 0,
    PhaseStationary = 1 << 1,
    PhaseChanged = 1 << 2,
    PhaseEnded = 1 << 3,
    PhaseCancelled = 1 << 4,
    PhaseMayBegin = 1 << 5,
  };

  float deltaX;
  float deltaY;
  float wheelTicksX;
  float wheelTicksY;

  float accelerationRatioX;
  float accelerationRatioY;

  // This field exists to allow BrowserPlugin to mark MouseWheel events as
  // 'resent' to handle the case where an event is not consumed when first
  // encountered; it should be handled differently by the plugin when it is
  // sent for thesecond time. No code within Blink touches this, other than to
  // plumb it through event conversions.
  int resendingPluginId;

  Phase phase;
  Phase momentumPhase;

  bool scrollByPage;
  bool hasPreciseScrollingDeltas;

  RailsMode railsMode;

  // Whether the event is blocking, non-blocking, all event
  // listeners were passive or was forced to be non-blocking.
  DispatchType dispatchType;

  WebMouseWheelEvent(Type type, int modifiers, double timeStampSeconds)
      : WebMouseEvent(sizeof(WebMouseWheelEvent),
                      type,
                      modifiers,
                      timeStampSeconds),
        deltaX(0.0f),
        deltaY(0.0f),
        wheelTicksX(0.0f),
        wheelTicksY(0.0f),
        accelerationRatioX(1.0f),
        accelerationRatioY(1.0f),
        resendingPluginId(-1),
        phase(PhaseNone),
        momentumPhase(PhaseNone),
        scrollByPage(false),
        hasPreciseScrollingDeltas(false),
        railsMode(RailsModeFree),
        dispatchType(Blocking) {}

  WebMouseWheelEvent()
      : WebMouseEvent(sizeof(WebMouseWheelEvent)),
        deltaX(0.0f),
        deltaY(0.0f),
        wheelTicksX(0.0f),
        wheelTicksY(0.0f),
        accelerationRatioX(1.0f),
        accelerationRatioY(1.0f),
        resendingPluginId(-1),
        phase(PhaseNone),
        momentumPhase(PhaseNone),
        railsMode(RailsModeFree),
        dispatchType(Blocking) {}

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT float deltaXInRootFrame() const;
  BLINK_PLATFORM_EXPORT float deltaYInRootFrame() const;

  // Sets any scaled values to be their computed values and sets |frameScale|
  // back to 1 and |translateX|, |translateY| back to 0.
  BLINK_PLATFORM_EXPORT WebMouseWheelEvent flattenTransform() const;

  bool isCancelable() const { return dispatchType == Blocking; }
#endif
};
#pragma pack(pop)

}  // namespace blink

#endif  // WebMouseWheelEvent_h
