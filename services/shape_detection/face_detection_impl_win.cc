// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_impl_win.h"

#include <windows.media.faceanalysis.h>

namespace shape_detection {

base::OnceCallback<void(bool)> g_callback_for_testing;

FaceDetectionImplWin::FaceDetectionImplWin(
    Microsoft::WRL::ComPtr<IFaceDetectorStatics> factory,
    Microsoft::WRL::ComPtr<IFaceDetector> face_detector)
    : face_detector_factory_(std::move(factory)),
      face_detector_(std::move(face_detector)) {}
FaceDetectionImplWin::~FaceDetectionImplWin() = default;

void FaceDetectionImplWin::Detect(const SkBitmap& bitmap,
                                  DetectCallback callback) {
  DCHECK_EQ(kN32_SkColorType, bitmap.colorType());

  if (g_callback_for_testing) {
    std::move(g_callback_for_testing).Run(face_detector_ ? true : false);
    std::move(callback).Run(std::vector<mojom::FaceDetectionResultPtr>());
  }
}

}  // namespace shape_detection