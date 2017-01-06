// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebMouseEvent.h"

namespace blink {

WebFloatPoint WebMouseEvent::movementInRootFrame() const {
  return WebFloatPoint((movementX / m_frameScale), (movementY / m_frameScale));
}

WebFloatPoint WebMouseEvent::positionInRootFrame() const {
  return WebFloatPoint((x / m_frameScale) + m_frameTranslate.x,
                       (y / m_frameScale) + m_frameTranslate.y);
}

void WebMouseEvent::flattenTransformSelf() {
  x = (x / m_frameScale) + m_frameTranslate.x;
  y = (y / m_frameScale) + m_frameTranslate.y;
  m_frameTranslate.x = 0;
  m_frameTranslate.y = 0;
  m_frameScale = 1;
}

}  // namespace blink
