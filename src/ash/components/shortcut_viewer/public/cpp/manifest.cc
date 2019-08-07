// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/shortcut_viewer/public/cpp/manifest.h"

#include "ash/components/shortcut_viewer/public/mojom/shortcut_viewer.mojom.h"
#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/ws/public/mojom/constants.mojom.h"
#include "ui/accessibility/mojom/ax_host.mojom.h"

namespace shortcut_viewer {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Keyboard Shortcut Viewer")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("none")
                           .Build())
          .ExposeCapability(
              mojom::kToggleUiCapability,
              service_manager::Manifest::InterfaceList<mojom::ShortcutViewer>())
          .RequireCapability(ax::mojom::kAXHostServiceName, "app")
          .RequireCapability(ws::mojom::kServiceName, "app")
          .Build()};
  return *manifest;
}

}  // namespace shortcut_viewer
