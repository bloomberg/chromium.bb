// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebTouchEvent.h"

namespace blink {

WebTouchEvent WebTouchEvent::flattenTransform() const {
  WebTouchEvent transformedEvent = *this;
  for (unsigned i = 0; i < touchesLength; ++i) {
    transformedEvent.touches[i] = touchPointInRootFrame(i);
  }
  transformedEvent.m_frameTranslate.x = 0;
  transformedEvent.m_frameTranslate.y = 0;
  transformedEvent.m_frameScale = 1;

  return transformedEvent;
}

WebTouchPoint WebTouchEvent::touchPointInRootFrame(unsigned point) const {
  DCHECK_LT(point, touchesLength);
  if (point >= touchesLength)
    return WebTouchPoint();

  WebTouchPoint transformedPoint = touches[point];
  transformedPoint.radiusX /= m_frameScale;
  transformedPoint.radiusY /= m_frameScale;
  transformedPoint.movementX /= m_frameScale;
  transformedPoint.movementY /= m_frameScale;
  transformedPoint.position.x =
      (transformedPoint.position.x / m_frameScale) + m_frameTranslate.x;
  transformedPoint.position.y =
      (transformedPoint.position.y / m_frameScale) + m_frameTranslate.y;
  return transformedPoint;
}

}  // namespace blink
