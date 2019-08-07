// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/test/echo/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/test/echo/public/mojom/echo.mojom.h"

namespace echo {

service_manager::Manifest GetManifest(
    service_manager::Manifest::ExecutionMode execution_mode) {
  return service_manager::ManifestBuilder()
      .WithServiceName(mojom::kServiceName)
      .WithDisplayName("Echo Service")
      .WithOptions(service_manager::ManifestOptionsBuilder()
                       .WithInstanceSharingPolicy(
                           service_manager::Manifest::InstanceSharingPolicy::
                               kSharedAcrossGroups)
                       .WithExecutionMode(execution_mode)
                       .Build())
      .ExposeCapability("echo",
                        service_manager::Manifest::InterfaceList<mojom::Echo>())
      .Build();
}

}  // namespace echo
