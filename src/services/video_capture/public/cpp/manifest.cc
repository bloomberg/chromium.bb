// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/manifest.h"

#if defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/mojo/cros_image_capture.mojom.h"
#endif  // defined(OS_CHROMEOS)
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"
#include "services/video_capture/public/mojom/testing_controls.mojom.h"

namespace video_capture {

service_manager::Manifest GetManifest(
    service_manager::Manifest::ExecutionMode execution_mode) {
  return service_manager::Manifest {
    service_manager::ManifestBuilder()
        .WithServiceName(mojom::kServiceName)
        .WithDisplayName("Video Capture")
        .WithOptions(service_manager::ManifestOptionsBuilder()
                         .WithExecutionMode(execution_mode)
                         .WithSandboxType("none")
                         .WithInstanceSharingPolicy(
                             service_manager::Manifest::InstanceSharingPolicy::
                                 kSharedAcrossGroups)
                         .Build())
        .ExposeCapability("capture", service_manager::Manifest::InterfaceList<
#if defined(OS_CHROMEOS)
                                         cros::mojom::CrosImageCapture,
#endif  // defined(OS_CHROMEOS)
                                         mojom::DeviceFactoryProvider>())
        .ExposeCapability(
            "tests",
            service_manager::Manifest::InterfaceList<
                mojom::DeviceFactoryProvider, mojom::TestingControls>())
        .Build()
  };
}

}  // namespace video_capture
