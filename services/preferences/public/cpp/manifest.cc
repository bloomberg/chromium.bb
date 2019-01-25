// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace prefs {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Preferences")
          .ExposeCapability("pref_client",
                            service_manager::Manifest::InterfaceList<
                                mojom::PrefStoreConnector>())
          .Build()};
  return *manifest;
}

}  // namespace prefs
