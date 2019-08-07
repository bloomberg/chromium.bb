// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/manifest.h"

#include "base/no_destructor.h"
#include "components/mirroring/mojom/constants.mojom.h"
#include "components/mirroring/mojom/mirroring_service.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace mirroring {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Mirroring Service")
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
                  .WithSandboxType("utility")
                  .Build())
          .ExposeCapability("mirroring",
                            service_manager::Manifest::InterfaceList<
                                mojom::MirroringService>())
          .RequireCapability("content_system", "gpu_client")
          .RequireCapability("ui", "gpu_client")

          .Build()};
  return *manifest;
}

}  // namespace mirroring
