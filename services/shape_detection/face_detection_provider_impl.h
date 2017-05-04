// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_SHAPE_DETECTION_FACE_DETECTION_PROVIDER_IMPL_H_
#define SERVICES_SHAPE_DETECTION_FACE_DETECTION_PROVIDER_IMPL_H_

#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/shape_detection/public/interfaces/facedetection_provider.mojom.h"

namespace service_manager {
struct BindSourceInfo;
}

namespace shape_detection {

class FaceDetectionProviderImpl
    : public shape_detection::mojom::FaceDetectionProvider {
 public:
  ~FaceDetectionProviderImpl() override = default;

  static void Create(
      const service_manager::BindSourceInfo& source_info,
      shape_detection::mojom::FaceDetectionProviderRequest request) {
    mojo::MakeStrongBinding(base::MakeUnique<FaceDetectionProviderImpl>(),
                            std::move(request));
  }

  void CreateFaceDetection(
      shape_detection::mojom::FaceDetectionRequest request,
      shape_detection::mojom::FaceDetectorOptionsPtr options) override;
};

}  // namespace shape_detection

#endif  // SERVICES_SHAPE_DETECTION_FACE_DETECTION_PROVIDER_IMPL_H_
