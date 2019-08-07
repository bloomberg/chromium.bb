// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/removable_storage_writer/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/removable_storage_writer/public/mojom/constants.mojom.h"
#include "chrome/services/removable_storage_writer/public/mojom/removable_storage_writer.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

const service_manager::Manifest& GetRemovableStorageWriterManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(chrome::mojom::kRemovableStorageWriterServiceName)
          .WithDisplayName(IDS_UTILITY_PROCESS_IMAGE_WRITER_NAME)
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
                  .WithSandboxType("none_and_elevated")
                  .WithInstanceSharingPolicy(
                      service_manager::Manifest::InstanceSharingPolicy::
                          kSharedAcrossGroups)
                  .Build())
          .ExposeCapability("removable_storage_writer",
                            service_manager::Manifest::InterfaceList<
                                chrome::mojom::RemovableStorageWriter>())

          .Build()};
  return *manifest;
}
