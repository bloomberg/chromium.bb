// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/secure_channel/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "chromeos/services/secure_channel/public/mojom/constants.mojom.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace chromeos {
namespace secure_channel {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("SecureChannel Service")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability(
              "secure_channel",
              service_manager::Manifest::InterfaceList<mojom::SecureChannel>())
          .Build()};
  return *manifest;
}

}  // namespace secure_channel
}  // namespace chromeos
