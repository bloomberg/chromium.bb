// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMouseEvent_h
#define WebMouseEvent_h

#include "WebInputEvent.h"

namespace blink {

// See WebInputEvent.h for details why this pack is here.
#pragma pack(push, 4)

// WebMouseEvent --------------------------------------------------------------

class WebMouseEvent : public WebInputEvent, public WebPointerProperties {
 public:
  // Renderer coordinates. Similar to viewport coordinates but without
  // DevTools emulation transform or overscroll applied. i.e. the coordinates
  // in Chromium's RenderView bounds.
  int x;
  int y;

  // DEPRECATED (crbug.com/507787)
  int windowX;
  int windowY;

  // Screen coordinate
  int globalX;
  int globalY;

  int movementX;
  int movementY;
  int clickCount;

  WebMouseEvent(Type type, int modifiers, double timeStampSeconds)
      : WebInputEvent(sizeof(WebMouseEvent), type, modifiers, timeStampSeconds),
        WebPointerProperties() {}

  WebMouseEvent()
      : WebInputEvent(sizeof(WebMouseEvent)), WebPointerProperties() {}

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebFloatPoint movementInRootFrame() const;
  BLINK_PLATFORM_EXPORT WebFloatPoint positionInRootFrame() const;
#endif

 protected:
  explicit WebMouseEvent(unsigned sizeParam)
      : WebInputEvent(sizeParam), WebPointerProperties() {}

  WebMouseEvent(unsigned sizeParam,
                Type type,
                int modifiers,
                double timeStampSeconds)
      : WebInputEvent(sizeParam, type, modifiers, timeStampSeconds),
        WebPointerProperties() {}

  void flattenTransformSelf();
};

#pragma pack(pop)

}  // namespace blink

#endif  // WebMouseEvent_h
