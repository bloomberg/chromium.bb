// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/wifi_util_win/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/services/wifi_util_win/public/mojom/constants.mojom.h"
#include "chrome/services/wifi_util_win/public/mojom/wifi_credentials_getter.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

const service_manager::Manifest& GetWifiUtilWinManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(chrome::mojom::kWifiUtilWinServiceName)
          .WithDisplayName(IDS_UTILITY_PROCESS_WIFI_CREDENTIALS_GETTER_NAME)
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
                  .WithSandboxType("none_and_elevated")
                  .WithInstanceSharingPolicy(
                      service_manager::Manifest::InstanceSharingPolicy::
                          kSharedAcrossGroups)
                  .Build())
          .ExposeCapability("wifi_credentials",
                            service_manager::Manifest::InterfaceList<
                                chrome::mojom::WiFiCredentialsGetter>())

          .Build()};
  return *manifest;
}
