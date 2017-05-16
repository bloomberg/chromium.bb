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
    kPhaseNone = 0,
    kPhaseBegan = 1 << 0,
    kPhaseStationary = 1 << 1,
    kPhaseChanged = 1 << 2,
    kPhaseEnded = 1 << 3,
    kPhaseCancelled = 1 << 4,
    kPhaseMayBegin = 1 << 5,
  };

  float delta_x;
  float delta_y;
  float wheel_ticks_x;
  float wheel_ticks_y;

  float acceleration_ratio_x;
  float acceleration_ratio_y;

  // This field exists to allow BrowserPlugin to mark MouseWheel events as
  // 'resent' to handle the case where an event is not consumed when first
  // encountered; it should be handled differently by the plugin when it is
  // sent for thesecond time. No code within Blink touches this, other than to
  // plumb it through event conversions.
  int resending_plugin_id;

  Phase phase;
  Phase momentum_phase;

  bool scroll_by_page;
  bool has_precise_scrolling_deltas;

  RailsMode rails_mode;

  // Whether the event is blocking, non-blocking, all event
  // listeners were passive or was forced to be non-blocking.
  DispatchType dispatch_type;

  WebMouseWheelEvent(Type type, int modifiers, double time_stamp_seconds)
      : WebMouseEvent(sizeof(WebMouseWheelEvent),
                      type,
                      modifiers,
                      time_stamp_seconds,
                      kMousePointerId),
        delta_x(0.0f),
        delta_y(0.0f),
        wheel_ticks_x(0.0f),
        wheel_ticks_y(0.0f),
        acceleration_ratio_x(1.0f),
        acceleration_ratio_y(1.0f),
        resending_plugin_id(-1),
        phase(kPhaseNone),
        momentum_phase(kPhaseNone),
        scroll_by_page(false),
        has_precise_scrolling_deltas(false),
        rails_mode(kRailsModeFree),
        dispatch_type(kBlocking) {}

  WebMouseWheelEvent()
      : WebMouseEvent(sizeof(WebMouseWheelEvent), kMousePointerId),
        delta_x(0.0f),
        delta_y(0.0f),
        wheel_ticks_x(0.0f),
        wheel_ticks_y(0.0f),
        acceleration_ratio_x(1.0f),
        acceleration_ratio_y(1.0f),
        resending_plugin_id(-1),
        phase(kPhaseNone),
        momentum_phase(kPhaseNone),
        rails_mode(kRailsModeFree),
        dispatch_type(kBlocking) {}

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT float DeltaXInRootFrame() const;
  BLINK_PLATFORM_EXPORT float DeltaYInRootFrame() const;

  // Sets any scaled values to be their computed values and sets |frameScale|
  // back to 1 and |translateX|, |translateY| back to 0.
  BLINK_PLATFORM_EXPORT WebMouseWheelEvent FlattenTransform() const;

  bool IsCancelable() const { return dispatch_type == kBlocking; }
#endif
};
#pragma pack(pop)

}  // namespace blink

#endif  // WebMouseWheelEvent_h
