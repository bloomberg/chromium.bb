// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebMouseWheelEvent.h"

namespace blink {

float WebMouseWheelEvent::deltaXInRootFrame() const {
  return deltaX / m_frameScale;
}

float WebMouseWheelEvent::deltaYInRootFrame() const {
  return deltaY / m_frameScale;
}

WebMouseWheelEvent WebMouseWheelEvent::flattenTransform() const {
  WebMouseWheelEvent result = *this;
  result.deltaX /= result.m_frameScale;
  result.deltaY /= result.m_frameScale;
  result.flattenTransformSelf();
  return result;
}

}  // namespace blink
