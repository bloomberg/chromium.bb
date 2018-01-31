// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/face_detection_provider_mac.h"

#include <memory>
#include <utility>

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shape_detection/face_detection_impl_mac.h"

namespace shape_detection {

FaceDetectionProviderMac::FaceDetectionProviderMac() = default;

FaceDetectionProviderMac::~FaceDetectionProviderMac() = default;

// static
void FaceDetectionProviderMac::Create(
    mojom::FaceDetectionProviderRequest request) {
  mojo::MakeStrongBinding(std::make_unique<FaceDetectionProviderMac>(),
                          std::move(request));
}

void FaceDetectionProviderMac::CreateFaceDetection(
    mojom::FaceDetectionRequest request,
    mojom::FaceDetectorOptionsPtr options) {
  mojo::MakeStrongBinding(
      std::make_unique<FaceDetectionImplMac>(std::move(options)),
      std::move(request));
}

}  // namespace shape_detection
