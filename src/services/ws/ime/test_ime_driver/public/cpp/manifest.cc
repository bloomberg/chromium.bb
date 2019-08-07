// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/ime/test_ime_driver/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/ws/ime/test_ime_driver/public/mojom/constants.mojom.h"
#include "services/ws/public/mojom/constants.mojom.h"

namespace test_ime_driver {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Test IME Driver")
          .ExposeCapability("ime:test_ime_driver", {})
          .RequireCapability(ws::mojom::kServiceName, "ime_registrar")
          .Build()};
  return *manifest;
}

}  // namespace test_ime_driver
