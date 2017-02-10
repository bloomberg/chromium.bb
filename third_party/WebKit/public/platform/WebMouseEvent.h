// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMouseEvent_h
#define WebMouseEvent_h

#include "WebInputEvent.h"

namespace blink {

class WebGestureEvent;

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

  int clickCount;

  WebMouseEvent(Type typeParam,
                int xParam,
                int yParam,
                int globalXParam,
                int globalYParam,
                int modifiersParam,
                double timeStampSecondsParam)
      : WebInputEvent(sizeof(WebMouseEvent),
                      typeParam,
                      modifiersParam,
                      timeStampSecondsParam),
        WebPointerProperties(),
        x(xParam),
        y(yParam),
        globalX(globalXParam),
        globalY(globalYParam) {}

  WebMouseEvent(Type typeParam,
                WebFloatPoint position,
                WebFloatPoint globalPosition,
                Button buttonParam,
                int clickCountParam,
                int modifiersParam,
                double timeStampSecondsParam)
      : WebInputEvent(sizeof(WebMouseEvent),
                      typeParam,
                      modifiersParam,
                      timeStampSecondsParam),
        WebPointerProperties(buttonParam, PointerType::Mouse),
        x(position.x),
        y(position.y),
        globalX(globalPosition.x),
        globalY(globalPosition.y),
        clickCount(clickCountParam) {}

  WebMouseEvent(Type typeParam,
                int modifiersParam,
                double timeStampSecondsParam)
      : WebMouseEvent(sizeof(WebMouseEvent),
                      typeParam,
                      modifiersParam,
                      timeStampSecondsParam) {}

  WebMouseEvent() : WebMouseEvent(sizeof(WebMouseEvent)) {}

  bool fromTouch() const {
    return (modifiers() & IsCompatibilityEventForTouch) != 0;
  }

#if INSIDE_BLINK
  BLINK_PLATFORM_EXPORT WebMouseEvent(Type typeParam,
                                      const WebGestureEvent&,
                                      Button buttonParam,
                                      int clickCountParam,
                                      int modifiersParam,
                                      double timeStampSecondsParam);

  BLINK_PLATFORM_EXPORT WebFloatPoint movementInRootFrame() const;
  BLINK_PLATFORM_EXPORT WebFloatPoint positionInRootFrame() const;

  // Sets any scaled values to be their computed values and sets |frameScale|
  // back to 1 and |translateX|, |translateY| back to 0.
  BLINK_PLATFORM_EXPORT WebMouseEvent flattenTransform() const;
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
