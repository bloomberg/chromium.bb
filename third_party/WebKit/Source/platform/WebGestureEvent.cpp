// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebGestureEvent.h"

namespace blink {

float WebGestureEvent::deltaXInRootFrame() const {
  if (m_type == WebInputEvent::GestureScrollBegin)
    return data.scrollBegin.deltaXHint / m_frameScale;
  DCHECK(m_type == WebInputEvent::GestureScrollUpdate);
  return data.scrollUpdate.deltaX / m_frameScale;
}

float WebGestureEvent::deltaYInRootFrame() const {
  if (m_type == WebInputEvent::GestureScrollBegin)
    return data.scrollBegin.deltaYHint / m_frameScale;
  DCHECK(m_type == WebInputEvent::GestureScrollUpdate);
  return data.scrollUpdate.deltaY / m_frameScale;
}

WebGestureEvent::ScrollUnits WebGestureEvent::deltaUnits() const {
  if (m_type == WebInputEvent::GestureScrollBegin)
    return data.scrollBegin.deltaHintUnits;
  if (m_type == WebInputEvent::GestureScrollUpdate)
    return data.scrollUpdate.deltaUnits;
  DCHECK(m_type == WebInputEvent::GestureScrollEnd);
  return data.scrollEnd.deltaUnits;
}

float WebGestureEvent::pinchScale() const {
  DCHECK(m_type == WebInputEvent::GesturePinchUpdate);
  return data.pinchUpdate.scale;
}

WebGestureEvent::InertialPhaseState WebGestureEvent::inertialPhase() const {
  if (m_type == WebInputEvent::GestureScrollBegin)
    return data.scrollBegin.inertialPhase;
  if (m_type == WebInputEvent::GestureScrollUpdate)
    return data.scrollUpdate.inertialPhase;
  DCHECK(m_type == WebInputEvent::GestureScrollEnd);
  return data.scrollEnd.inertialPhase;
}

bool WebGestureEvent::synthetic() const {
  if (m_type == WebInputEvent::GestureScrollBegin)
    return data.scrollBegin.synthetic;
  DCHECK(m_type == WebInputEvent::GestureScrollEnd);
  return data.scrollEnd.synthetic;
}

float WebGestureEvent::velocityX() const {
  if (m_type == WebInputEvent::GestureScrollUpdate)
    return data.scrollUpdate.velocityX;
  DCHECK(m_type == WebInputEvent::GestureFlingStart);
  return data.flingStart.velocityX;
}

float WebGestureEvent::velocityY() const {
  if (m_type == WebInputEvent::GestureScrollUpdate)
    return data.scrollUpdate.velocityY;
  DCHECK(m_type == WebInputEvent::GestureFlingStart);
  return data.flingStart.velocityY;
}

WebFloatSize WebGestureEvent::tapAreaInRootFrame() const {
  if (m_type == WebInputEvent::GestureTwoFingerTap) {
    return WebFloatSize(data.twoFingerTap.firstFingerWidth / m_frameScale,
                        data.twoFingerTap.firstFingerHeight / m_frameScale);
  } else if (m_type == WebInputEvent::GestureLongPress ||
             m_type == WebInputEvent::GestureLongTap) {
    return WebFloatSize(data.longPress.width / m_frameScale,
                        data.longPress.height / m_frameScale);
  } else if (m_type == WebInputEvent::GestureTap ||
             m_type == WebInputEvent::GestureTapUnconfirmed) {
    return WebFloatSize(data.tap.width / m_frameScale,
                        data.tap.height / m_frameScale);
  } else if (m_type == WebInputEvent::GestureTapDown) {
    return WebFloatSize(data.tapDown.width / m_frameScale,
                        data.tapDown.height / m_frameScale);
  } else if (m_type == WebInputEvent::GestureShowPress) {
    return WebFloatSize(data.showPress.width / m_frameScale,
                        data.showPress.height / m_frameScale);
  }
  // This function is called for all gestures and determined if the tap
  // area is empty or not; so return an empty rect here.
  return WebFloatSize();
}

WebFloatPoint WebGestureEvent::positionInRootFrame() const {
  return WebFloatPoint((x / m_frameScale) + m_frameTranslate.x,
                       (y / m_frameScale) + m_frameTranslate.y);
}

int WebGestureEvent::tapCount() const {
  DCHECK(m_type == WebInputEvent::GestureTap);
  return data.tap.tapCount;
}

void WebGestureEvent::applyTouchAdjustment(WebFloatPoint rootFrameCoords) {
  // Update the window-relative position of the event so that the node that
  // was ultimately hit is under this point (i.e. elementFromPoint for the
  // client co-ordinates in a 'click' event should yield the target). The
  // global position is intentionally left unmodified because it's intended to
  // reflect raw co-ordinates unrelated to any content.
  m_frameTranslate.x = rootFrameCoords.x - (x / m_frameScale);
  m_frameTranslate.y = rootFrameCoords.y - (y / m_frameScale);
}

void WebGestureEvent::flattenTransform() {
  if (m_frameScale != 1) {
    switch (m_type) {
      case WebInputEvent::GestureScrollBegin:
        data.scrollBegin.deltaXHint /= m_frameScale;
        data.scrollBegin.deltaYHint /= m_frameScale;
        break;
      case WebInputEvent::GestureScrollUpdate:
        data.scrollUpdate.deltaX /= m_frameScale;
        data.scrollUpdate.deltaY /= m_frameScale;
        break;
      case WebInputEvent::GestureTwoFingerTap:
        data.twoFingerTap.firstFingerWidth /= m_frameScale;
        data.twoFingerTap.firstFingerHeight /= m_frameScale;
        break;
      case WebInputEvent::GestureLongPress:
      case WebInputEvent::GestureLongTap:
        data.longPress.width /= m_frameScale;
        data.longPress.height /= m_frameScale;
        break;
      case WebInputEvent::GestureTap:
      case WebInputEvent::GestureTapUnconfirmed:
        data.tap.width /= m_frameScale;
        data.tap.height /= m_frameScale;
        break;
      case WebInputEvent::GestureTapDown:
        data.tapDown.width /= m_frameScale;
        data.tapDown.height /= m_frameScale;
        break;
      case WebInputEvent::GestureShowPress:
        data.showPress.width /= m_frameScale;
        data.showPress.height /= m_frameScale;
        break;
      default:
        break;
    }
  }

  x = (x / m_frameScale) + m_frameTranslate.x;
  y = (y / m_frameScale) + m_frameTranslate.y;
  m_frameTranslate.x = 0;
  m_frameTranslate.y = 0;
  m_frameScale = 1;
}

}  // namespace blink
