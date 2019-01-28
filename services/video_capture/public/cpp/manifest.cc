// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/device_factory_provider.mojom.h"
#include "services/video_capture/public/mojom/testing_controls.mojom.h"

namespace video_capture {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Video Capture")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("none")
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability("capture", service_manager::Manifest::InterfaceList<
                                           mojom::DeviceFactoryProvider>())
          .ExposeCapability(
              "tests",
              service_manager::Manifest::InterfaceList<
                  mojom::DeviceFactoryProvider, mojom::TestingControls>())
          .Build()};
  return *manifest;
}

}  // namespace video_capture
