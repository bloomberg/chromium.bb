// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/test/user_id/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/test/user_id/public/mojom/user_id.mojom.h"

namespace user_id {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName("user_id")
          .WithDisplayName("User ID Service")
          .WithOptions(service_manager::ManifestOptionsBuilder().Build())
          .ExposeCapability(
              "user_id",
              service_manager::Manifest::InterfaceList<mojom::UserId>())
          .Build()};
  return *manifest;
}

}  // namespace user_id
