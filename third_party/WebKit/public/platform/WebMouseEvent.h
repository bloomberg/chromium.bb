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

// TODO(mustaq): We are truncating |float|s to integers whenever setting
//   coordinate values, to avoid regressions for now. Will be fixed later
//   on. crbug.com/456625
class WebMouseEvent : public WebInputEvent, public WebPointerProperties {
 public:
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
        m_positionInWidget(xParam, yParam),
        m_positionInScreen(globalXParam, globalYParam) {}

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
        clickCount(clickCountParam),
        m_positionInWidget(floor(position.x), floor(position.y)),
        m_positionInScreen(floor(globalPosition.x), floor(globalPosition.y)) {}

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

  WebFloatPoint positionInWidget() const { return m_positionInWidget; }
  void setPositionInWidget(float x, float y) {
    m_positionInWidget = WebFloatPoint(floor(x), floor(y));
  }

  WebFloatPoint positionInScreen() const { return m_positionInScreen; }
  void setPositionInScreen(float x, float y) {
    m_positionInScreen = WebFloatPoint(floor(x), floor(y));
  }

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

 private:
  // Widget coordinate, which is relative to the bound of current RenderWidget
  // (e.g. a plugin or OOPIF inside a RenderView). Similar to viewport
  // coordinates but without DevTools emulation transform or overscroll applied.
  WebFloatPoint m_positionInWidget;

  // Screen coordinate
  WebFloatPoint m_positionInScreen;
};

#pragma pack(pop)

}  // namespace blink

#endif  // WebMouseEvent_h
