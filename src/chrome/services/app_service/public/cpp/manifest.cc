// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "chrome/services/app_service/public/mojom/constants.mojom.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace apps {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("App Service")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("none")
                           .Build())
          .ExposeCapability(
              "app_service",
              service_manager::Manifest::InterfaceList<mojom::AppService>())
          .RequireCapability(prefs::mojom::kServiceName, "pref_client")
          .Build()};
  return *manifest;
}

}  // namespace apps
