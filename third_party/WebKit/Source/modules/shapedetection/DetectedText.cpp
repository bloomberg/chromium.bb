// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/DetectedText.h"

#include "core/dom/DOMRect.h"

namespace blink {

DetectedText* DetectedText::create() {
  return new DetectedText(emptyString, DOMRect::create());
}

DetectedText* DetectedText::create(String rawValue, DOMRect* boundingBox) {
  return new DetectedText(rawValue, boundingBox);
}

const String& DetectedText::rawValue() const {
  return m_rawValue;
}

DOMRect* DetectedText::boundingBox() const {
  return m_boundingBox.get();
}

DetectedText::DetectedText(String rawValue, DOMRect* boundingBox)
    : m_rawValue(rawValue), m_boundingBox(boundingBox) {}

DEFINE_TRACE(DetectedText) {
  visitor->trace(m_boundingBox);
}

}  // namespace blink
