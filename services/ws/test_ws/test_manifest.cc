// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/test_ws/test_manifest.h"

#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/ws/public/cpp/manifest.h"
#include "services/ws/test_ws/test_ws.mojom.h"

namespace test_ws {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Test Window Service")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("none")
                           .Build())
          .ExposeCapability(
              "test", service_manager::Manifest::InterfaceList<mojom::TestWs>())
          .PackageService(ws::GetManifest())
          .Build()};
  return *manifest;
}

}  // namespace test_ws
