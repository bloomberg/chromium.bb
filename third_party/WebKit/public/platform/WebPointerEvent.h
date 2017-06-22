// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebPointerEvent_h
#define WebPointerEvent_h

#include "WebInputEvent.h"
#include "WebPointerProperties.h"
#include "WebTouchEvent.h"

namespace blink {

// See WebInputEvent.h for details why this pack is here.
#pragma pack(push, 4)

// WebPointerEvent
// This is a WIP and currently used only in Blink and only for touch.
// TODO(nzolghadr): We should unify the fields in this class into
// WebPointerProperties and not have pointertype specific attributes here.
// --------------------------------------------------------------

class WebPointerEvent : public WebInputEvent, public WebPointerProperties {
 public:
  WebPointerEvent()
      : WebInputEvent(sizeof(WebPointerEvent)), WebPointerProperties(0) {}
  BLINK_PLATFORM_EXPORT WebPointerEvent(const WebTouchEvent&,
                                        const WebTouchPoint&);

  // TODO(crbug.com/736014): We need a clarified definition of the scale and
  // the coordinate space on these attributes.
  float width;
  float height;

  // ------------ Touch Point Specific ------------

  float rotation_angle;

  // ------------ Touch Event Specific ------------

  // Whether the event is blocking, non-blocking, all event
  // listeners were passive or was forced to be non-blocking.
  DispatchType dispatch_type;

  // For a single touch, this is true after the touch-point has moved beyond
  // the platform slop region. For a multitouch, this is true after any
  // touch-point has moved (by whatever amount).
  bool moved_beyond_slop_region;

  // Whether this touch event is a touchstart or a first touchmove event per
  // scroll.
  bool touch_start_or_first_touch_move;

#if INSIDE_BLINK
  bool IsCancelable() const { return dispatch_type == kBlocking; }

  BLINK_PLATFORM_EXPORT WebPointerEvent WebPointerEventInRootFrame() const;

#endif
};

#pragma pack(pop)

}  // namespace blink

#endif  // WebMouseEvent_h
