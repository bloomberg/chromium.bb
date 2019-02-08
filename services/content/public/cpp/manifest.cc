// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/content/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/content/public/mojom/constants.mojom.h"
#include "services/content/public/mojom/navigable_contents_factory.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace content {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Content Service")
          .ExposeCapability("navigation",
                            service_manager::Manifest::InterfaceList<
                                mojom::NavigableContentsFactory>())
          .Build()};
  return *manifest;
}

}  // namespace content
