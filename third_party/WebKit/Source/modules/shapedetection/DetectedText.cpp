// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/DetectedText.h"

#include "core/geometry/DOMRect.h"

namespace blink {

DetectedText* DetectedText::Create() {
  return new DetectedText(g_empty_string, DOMRect::Create());
}

DetectedText* DetectedText::Create(String raw_value, DOMRect* bounding_box) {
  return new DetectedText(raw_value, bounding_box);
}

const String& DetectedText::rawValue() const {
  return raw_value_;
}

DOMRect* DetectedText::boundingBox() const {
  return bounding_box_.Get();
}

DetectedText::DetectedText(String raw_value, DOMRect* bounding_box)
    : raw_value_(raw_value), bounding_box_(bounding_box) {}

DEFINE_TRACE(DetectedText) {
  visitor->Trace(bounding_box_);
}

}  // namespace blink
