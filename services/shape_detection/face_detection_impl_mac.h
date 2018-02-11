// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_H_
#define SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_H_

#include "base/mac/scoped_nsobject.h"
#include "services/shape_detection/public/mojom/facedetection.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

@class CIDetector;

namespace shape_detection {

class FaceDetectionImplMac : public shape_detection::mojom::FaceDetection {
 public:
  explicit FaceDetectionImplMac(
      shape_detection::mojom::FaceDetectorOptionsPtr options);
  ~FaceDetectionImplMac() override;

  void Detect(
      const SkBitmap& bitmap,
      shape_detection::mojom::FaceDetection::DetectCallback callback) override;

 private:
  base::scoped_nsobject<CIDetector> detector_;

  DISALLOW_COPY_AND_ASSIGN(FaceDetectionImplMac);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_H_
