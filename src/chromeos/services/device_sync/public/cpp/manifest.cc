// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "chromeos/services/device_sync/public/mojom/constants.mojom.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace chromeos {
namespace device_sync {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("DeviceSync Service")
          .ExposeCapability(
              "device_sync",
              service_manager::Manifest::InterfaceList<mojom::DeviceSync>())
          .RequireCapability(prefs::mojom::kServiceName, "pref_client")
          .Build()};
  return *manifest;
}

}  // namespace device_sync
}  // namespace chromeos
