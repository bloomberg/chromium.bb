// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/image_annotation/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/data_decoder/public/mojom/constants.mojom.h"
#include "services/image_annotation/public/mojom/constants.mojom.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace image_annotation {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Image Annotation Service")
          .ExposeCapability(
              mojom::kAnnotationCapability,
              service_manager::Manifest::InterfaceList<mojom::Annotator>())
          .RequireCapability(data_decoder::mojom::kServiceName, "json_parser")
          .Build()};
  return *manifest;
}

}  // namespace image_annotation
