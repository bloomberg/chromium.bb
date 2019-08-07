// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/quarantine/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "services/service_manager/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

#if defined(OS_WIN)
#include "components/services/quarantine/public/cpp/quarantine_features_win.h"
#endif

namespace quarantine {

service_manager::Manifest::ExecutionMode GetExecutionMode() {
#if defined(OS_WIN)
  if (base::FeatureList::IsEnabled(quarantine::kOutOfProcessQuarantine))
    return service_manager::Manifest::ExecutionMode::kOutOfProcessBuiltin;
#endif
  return service_manager::Manifest::ExecutionMode::kInProcessBuiltin;
}

const service_manager::Manifest& GetQuarantineManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Quarantine Service")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithExecutionMode(GetExecutionMode())
                           .WithSandboxType("none")
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability(
              mojom::kQuarantineFileCapability,
              service_manager::Manifest::InterfaceList<mojom::Quarantine>())
          .Build()};
  return *manifest;
}

}  // namespace quarantine
