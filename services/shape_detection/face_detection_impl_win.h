// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_WIN_H_
#define SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_WIN_H_

#include <wrl/client.h>

#include "services/shape_detection/public/interfaces/facedetection.mojom.h"

class SkBitmap;

namespace ABI {
namespace Windows {
namespace Media {
namespace FaceAnalysis {
struct IFaceDetectorStatics;
}  // namespace FaceAnalysis
}  // namespace Media
}  // namespace Windows
}  // namespace ABI

namespace shape_detection {

class FaceDetectionImplWin : public mojom::FaceDetection {
 public:
  using IFaceDetectorStatics =
      ABI::Windows::Media::FaceAnalysis::IFaceDetectorStatics;

  static std::unique_ptr<FaceDetectionImplWin> Create();

  explicit FaceDetectionImplWin(
      Microsoft::WRL::ComPtr<IFaceDetectorStatics> factory);
  ~FaceDetectionImplWin() override;

  void Detect(const SkBitmap& bitmap,
              mojom::FaceDetection::DetectCallback callback) override;

 private:
  Microsoft::WRL::ComPtr<IFaceDetectorStatics> face_detector_factory_;

  DISALLOW_COPY_AND_ASSIGN(FaceDetectionImplWin);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_WIN_H_