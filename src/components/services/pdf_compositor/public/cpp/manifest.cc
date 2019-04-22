// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/pdf_compositor/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "components/services/pdf_compositor/public/interfaces/pdf_compositor.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace printing {

const service_manager::Manifest& GetPdfCompositorManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("PDF Compositor Service")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("pdf_compositor")
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability(
              "compositor",
              service_manager::Manifest::InterfaceList<mojom::PdfCompositor>())
          .RequireCapability("content_browser", "app")
          .RequireCapability("content_browser", "sandbox_support")
          .RequireCapability("ui", "discardable_memory")
          .Build()};
  return *manifest;
}

}  // namespace printing
