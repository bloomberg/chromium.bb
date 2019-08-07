// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/prefs/ash_pref_connector_manifest.h"

#include "ash/public/interfaces/pref_connector.mojom.h"
#include "base/no_destructor.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

const service_manager::Manifest& GetAshPrefConnectorManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(ash::mojom::kPrefConnectorServiceName)
          .WithDisplayName("Ash Pref Connector")
          .ExposeCapability("pref_connector",
                            service_manager::Manifest::InterfaceList<
                                ash::mojom::PrefConnector>())
          .RequireCapability(prefs::mojom::kServiceName, "pref_client")
          .Build()};
  return *manifest;
}
