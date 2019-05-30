// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/manifest.h"

#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/network_service_test.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace network {

service_manager::Manifest GetManifest(
    service_manager::Manifest::ExecutionMode execution_mode) {
  return service_manager::Manifest{
      service_manager::ManifestBuilder()
          .WithServiceName("network")
          .WithDisplayName("Network Service")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithExecutionMode(execution_mode)
                           .WithSandboxType("network")
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability("test", service_manager::Manifest::InterfaceList<
                                        mojom::NetworkServiceTest>())
          .ExposeCapability(
              "network_service",
              service_manager::Manifest::InterfaceList<mojom::NetworkService>())
          .Build()};
}

}  // namespace network
