// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/DetectedFace.h"

#include "core/geometry/DOMRect.h"

namespace blink {

DetectedFace* DetectedFace::Create() {
  return new DetectedFace(DOMRect::Create());
}

DetectedFace* DetectedFace::Create(DOMRect* bounding_box) {
  return new DetectedFace(bounding_box);
}

DOMRect* DetectedFace::boundingBox() const {
  return bounding_box_.Get();
}

DetectedFace::DetectedFace(DOMRect* bounding_box)
    : bounding_box_(bounding_box) {}

DEFINE_TRACE(DetectedFace) {
  visitor->Trace(bounding_box_);
}

}  // namespace blink
