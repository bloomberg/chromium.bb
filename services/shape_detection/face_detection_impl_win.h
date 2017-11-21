// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_WIN_H_
#define SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_WIN_H_

#include <windows.foundation.h>
#include <wrl/client.h>

#include "detection_utils_win.h"
#include "services/shape_detection/public/interfaces/facedetection.mojom.h"

class SkBitmap;

namespace ABI {
namespace Windows {
namespace Media {
namespace FaceAnalysis {
interface IFaceDetectorStatics;
interface IFaceDetector;
class FaceDetector;
}  // namespace FaceAnalysis
}  // namespace Media
}  // namespace Windows
}  // namespace ABI

namespace shape_detection {

extern base::OnceCallback<void(bool)> g_callback_for_testing;

class FaceDetectionImplWin : public mojom::FaceDetection {
 public:
  using IFaceDetectorStatics =
      ABI::Windows::Media::FaceAnalysis::IFaceDetectorStatics;
  using FaceDetector = ABI::Windows::Media::FaceAnalysis::FaceDetector;
  using IFaceDetector = ABI::Windows::Media::FaceAnalysis::IFaceDetector;

  FaceDetectionImplWin(Microsoft::WRL::ComPtr<IFaceDetectorStatics> factory,
                       Microsoft::WRL::ComPtr<IFaceDetector> face_detector);
  ~FaceDetectionImplWin() override;

  // mojom::FaceDetection implementation.
  void Detect(const SkBitmap& bitmap,
              mojom::FaceDetection::DetectCallback callback) override;

 private:
  Microsoft::WRL::ComPtr<IFaceDetectorStatics> face_detector_factory_;
  Microsoft::WRL::ComPtr<IFaceDetector> face_detector_;

  DISALLOW_COPY_AND_ASSIGN(FaceDetectionImplWin);
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_FACE_DETECTION_IMPL_WIN_H_