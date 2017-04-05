// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebMouseEvent.h"

#include "public/platform/WebGestureEvent.h"

namespace blink {

WebMouseEvent::WebMouseEvent(WebInputEvent::Type type,
                             const WebGestureEvent& gestureEvent,
                             Button buttonParam,
                             int clickCountParam,
                             int modifiers,
                             double timeStampSeconds)
    : WebInputEvent(sizeof(WebMouseEvent), type, modifiers, timeStampSeconds),
      WebPointerProperties(buttonParam,
                           WebPointerProperties::PointerType::Mouse),
      clickCount(clickCountParam),
      m_positionInWidget(gestureEvent.x, gestureEvent.y),
      m_positionInScreen(gestureEvent.globalX, gestureEvent.globalY) {
  setFrameScale(gestureEvent.frameScale());
  setFrameTranslate(gestureEvent.frameTranslate());
}

WebFloatPoint WebMouseEvent::movementInRootFrame() const {
  return WebFloatPoint((movementX / m_frameScale), (movementY / m_frameScale));
}

WebFloatPoint WebMouseEvent::positionInRootFrame() const {
  return WebFloatPoint(
      (m_positionInWidget.x / m_frameScale) + m_frameTranslate.x,
      (m_positionInWidget.y / m_frameScale) + m_frameTranslate.y);
}

WebMouseEvent WebMouseEvent::flattenTransform() const {
  WebMouseEvent result = *this;
  result.flattenTransformSelf();
  return result;
}

void WebMouseEvent::flattenTransformSelf() {
  m_positionInWidget.x =
      floor((m_positionInWidget.x / m_frameScale) + m_frameTranslate.x);
  m_positionInWidget.y =
      floor((m_positionInWidget.y / m_frameScale) + m_frameTranslate.y);
  m_frameTranslate.x = 0;
  m_frameTranslate.y = 0;
  m_frameScale = 1;
}

}  // namespace blink
