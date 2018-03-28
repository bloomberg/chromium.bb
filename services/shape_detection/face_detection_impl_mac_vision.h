// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_VISION_H_
#define SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_VISION_H_

#include <memory>
#include <utility>

#include "base/mac/availability.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
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

  void SetBinding(mojo::StrongBindingPtr<mojom::FaceDetection> binding) {
    binding_ = std::move(binding);
  }

 private:
  class VisionAPIAsyncRequestMac;
  void OnFacesDetected(VNRequest* request, NSError* error);

  CGSize image_size_;
  std::unique_ptr<VisionAPIAsyncRequestMac> landmarks_async_request_;
  DetectCallback detected_callback_;
  mojo::StrongBindingPtr<mojom::FaceDetection> binding_;
  base::WeakPtrFactory<FaceDetectionImplMacVision> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FaceDetectionImplMacVision);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_MAC_VISION_H_
