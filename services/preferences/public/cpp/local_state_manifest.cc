// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/local_state_manifest.h"

#include "base/no_destructor.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace prefs {

const service_manager::Manifest& GetLocalStateManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kLocalStateServiceName)
          .WithDisplayName("Local state preferences")
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
                  .WithSandboxType("utility")
                  .Build())
          .ExposeCapability("pref_client",
                            service_manager::Manifest::InterfaceList<
                                mojom::PrefStoreConnector>())
          .Build()};
  return *manifest;
}

}  // namespace prefs
