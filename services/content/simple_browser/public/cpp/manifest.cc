// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/simple_browser/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/content/simple_browser/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace simple_browser {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Simple Browser")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("utility")
                           .Build())
          .ExposeCapability("app", {})
          .RequireCapability("content", "navigation")
          .RequireCapability("font_service", "font_service")
          .RequireCapability("ui", "app")
          .Build()};
  return *manifest;
}

}  // namespace simple_browser
