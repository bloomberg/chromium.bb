// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/viz/privileged/interfaces/viz_main.mojom.h"
#include "services/viz/public/interfaces/constants.mojom.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/interfaces/device_cursor.mojom.h"
#include "ui/ozone/public/interfaces/drm_device.mojom.h"
#endif

namespace viz {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .WithServiceName(mojom::kVizServiceName)
        .WithDisplayName("Visuals Service")
        .WithOptions(
            service_manager::ManifestOptionsBuilder()
                .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                       kOutOfProcessBuiltin)
                // The viz service sometimes needs to do some additional work
                // before entering the sandbox. So set sandbox type to "none",
                // so that the service manager does not enter the sandbox. The
                // viz service itself enters the sandbox at the right time. See
                // code in GpuSandboxHelper for more details.
                // TODO(crbug.com/708738): Revisit the implementation once
                // sandboxing is fixed.
                .WithSandboxType("none")
                .WithInstanceSharingPolicy(
                    service_manager::Manifest::InstanceSharingPolicy::
                        kSharedAcrossGroups)
                .Build())
#if defined(USE_OZONE)
        .ExposeCapability(
            "ozone",
            service_manager::Manifest::InterfaceList<
                ui::ozone::mojom::DeviceCursor, ui::ozone::mojom::DrmDevice>())
#endif
        .ExposeCapability(
            "viz_host",
            service_manager::Manifest::InterfaceList<mojom::VizMain>())
        .RequireCapability("metrics", "url_keyed_metrics")
        .RequireCapability("ui", "ozone")
        .Build()
  };
  return *manifest;
}

}  // namespace viz
