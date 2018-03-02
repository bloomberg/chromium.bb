// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_VISION_H_
#define SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_VISION_H_

#include "base/mac/availability.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#include "services/shape_detection/public/mojom/facedetection.mojom.h"

class SkBitmap;

namespace shape_detection {

// The FaceDetectionImplMacVision class is the implementation of Face Detection
// based on Mac OS Vision framework.
class API_AVAILABLE(macos(10.13)) FaceDetectionImplMacVision
    : public mojom::FaceDetection {
 public:
  FaceDetectionImplMacVision();
  ~FaceDetectionImplMacVision() override;

  void Detect(const SkBitmap& bitmap,
              mojom::FaceDetection::DetectCallback callback) override;

 private:
  base::scoped_nsobject<VNDetectFaceLandmarksRequest> landmarks_request_;

  DISALLOW_COPY_AND_ASSIGN(FaceDetectionImplMacVision);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_VISION_H_
