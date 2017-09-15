// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/DetectedText.h"

#include "core/geometry/DOMRect.h"

namespace blink {

DetectedText* DetectedText::Create() {
  HeapVector<Point2D> empty_list;
  return new DetectedText(g_empty_string, DOMRect::Create(), empty_list);
}

DetectedText* DetectedText::Create(String raw_value,
                                   DOMRect* bounding_box,
                                   HeapVector<Point2D> corner_points) {
  return new DetectedText(raw_value, bounding_box, corner_points);
}

const String& DetectedText::rawValue() const {
  return raw_value_;
}

DOMRect* DetectedText::boundingBox() const {
  return bounding_box_.Get();
}

const HeapVector<Point2D>& DetectedText::cornerPoints() const {
  return corner_points_;
}

DetectedText::DetectedText(String raw_value,
                           DOMRect* bounding_box,
                           HeapVector<Point2D> corner_points)
    : raw_value_(raw_value),
      bounding_box_(bounding_box),
      corner_points_(corner_points) {}

DEFINE_TRACE(DetectedText) {
  visitor->Trace(bounding_box_);
  visitor->Trace(corner_points_);
}

}  // namespace blink
