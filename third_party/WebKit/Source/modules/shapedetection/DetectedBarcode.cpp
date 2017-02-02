// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/DetectedBarcode.h"

#include "core/dom/DOMRect.h"

namespace blink {

DetectedBarcode* DetectedBarcode::create() {
  HeapVector<Point2D> emptyList;
  return new DetectedBarcode(emptyString, DOMRect::create(0, 0, 0, 0),
                             emptyList);
}

DetectedBarcode* DetectedBarcode::create(String rawValue,
                                         DOMRect* boundingBox,
                                         HeapVector<Point2D> cornerPoints) {
  return new DetectedBarcode(rawValue, boundingBox, cornerPoints);
}

const String& DetectedBarcode::rawValue() const {
  return m_rawValue;
}

DOMRect* DetectedBarcode::boundingBox() const {
  return m_boundingBox.get();
}

const HeapVector<Point2D>& DetectedBarcode::cornerPoints() const {
  return m_cornerPoints;
}

DetectedBarcode::DetectedBarcode(String rawValue,
                                 DOMRect* boundingBox,
                                 HeapVector<Point2D> cornerPoints)
    : m_rawValue(rawValue),
      m_boundingBox(boundingBox),
      m_cornerPoints(cornerPoints) {}

DEFINE_TRACE(DetectedBarcode) {
  visitor->trace(m_boundingBox);
  visitor->trace(m_cornerPoints);
}

}  // namespace blink
