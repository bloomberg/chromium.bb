// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_host_manifest.h"

#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "ui/accessibility/mojom/ax_host.mojom.h"

namespace ui {

const service_manager::Manifest& GetAXHostManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(ax::mojom::kAXHostServiceName)
          .WithDisplayName("Accessibility Host Service")
          .WithOptions(service_manager::ManifestOptionsBuilder().Build())
          .ExposeCapability(
              "app",
              service_manager::Manifest::InterfaceList<ax::mojom::AXHost>())
          .Build()};
  return *manifest;
}

}  // namespace ui
