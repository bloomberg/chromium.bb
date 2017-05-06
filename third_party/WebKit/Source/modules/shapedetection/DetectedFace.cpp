// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/shapedetection/DetectedFace.h"

#include "core/geometry/DOMRect.h"
#include "modules/shapedetection/Landmark.h"

namespace blink {

DetectedFace* DetectedFace::Create() {
  return new DetectedFace(DOMRect::Create());
}

DetectedFace* DetectedFace::Create(DOMRect* bounding_box) {
  return new DetectedFace(bounding_box);
}

DetectedFace* DetectedFace::Create(DOMRect* bounding_box,
                                   const HeapVector<Landmark>& landmarks) {
  return new DetectedFace(bounding_box, landmarks);
}

DOMRect* DetectedFace::boundingBox() const {
  return bounding_box_.Get();
}

const HeapVector<Landmark>& DetectedFace::landmarks() const {
  return landmarks_;
}

DetectedFace::DetectedFace(DOMRect* bounding_box)
    : bounding_box_(bounding_box) {}

DetectedFace::DetectedFace(DOMRect* bounding_box,
                           const HeapVector<Landmark>& landmarks)
    : bounding_box_(bounding_box), landmarks_(landmarks) {}

DEFINE_TRACE(DetectedFace) {
  visitor->Trace(bounding_box_);
  visitor->Trace(landmarks_);
}

}  // namespace blink
