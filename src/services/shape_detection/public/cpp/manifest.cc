// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/shape_detection/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"
#include "services/shape_detection/public/mojom/constants.mojom.h"
#include "services/shape_detection/public/mojom/facedetection_provider.mojom.h"
#include "services/shape_detection/public/mojom/textdetection.mojom.h"

namespace shape_detection {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Shape Detection Service")
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
                  .WithInstanceSharingPolicy(
                      service_manager::Manifest::InstanceSharingPolicy::
                          kSharedAcrossGroups)
                  .Build())
          .ExposeCapability("barcode_detection",
                            service_manager::Manifest::InterfaceList<
                                mojom::BarcodeDetectionProvider>())
          .ExposeCapability("face_detection",
                            service_manager::Manifest::InterfaceList<
                                mojom::FaceDetectionProvider>())
          .ExposeCapability(
              "text_detection",
              service_manager::Manifest::InterfaceList<mojom::TextDetection>())
          .Build()};
  return *manifest;
}

}  // namespace shape_detection
