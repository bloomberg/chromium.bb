// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/importer/profile_import_manifest.h"

#include "base/no_destructor.h"
#include "chrome/common/importer/profile_import.mojom.h"
#include "chrome/grit/generated_resources.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

const service_manager::Manifest& GetProfileImportManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(chrome::mojom::kProfileImportServiceName)
          .WithDisplayName(IDS_UTILITY_PROCESS_PROFILE_IMPORTER_NAME)
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
                  .WithSandboxType("none")
                  .WithInstanceSharingPolicy(
                      service_manager::Manifest::InstanceSharingPolicy::
                          kSharedAcrossGroups)
                  .Build())
          .ExposeCapability("import", service_manager::Manifest::InterfaceList<
                                          chrome::mojom::ProfileImport>())
          .Build()};
  return *manifest;
}
