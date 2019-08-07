// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/identity/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/identity/public/mojom/identity_accessor.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace identity {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Identity Service")
          .ExposeCapability("identity_accessor",
                            service_manager::Manifest::InterfaceList<
                                mojom::IdentityAccessor>())
          .Build()};
  return *manifest;
}

}  // namespace identity
