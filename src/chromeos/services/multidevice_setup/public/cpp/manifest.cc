// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace chromeos {
namespace multidevice_setup {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("MultiDevice Setup Service")
          .WithOptions(service_manager::ManifestOptionsBuilder().Build())
          .ExposeCapability(
              "multidevice_setup",
              service_manager::Manifest::InterfaceList<
                  mojom::MultiDeviceSetup, mojom::PrivilegedHostDeviceSetter>())
          .Build()};
  return *manifest;
}

}  // namespace multidevice_setup
}  // namespace chromeos
