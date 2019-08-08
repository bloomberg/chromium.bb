// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_browser_manifest.h"

#include "base/no_destructor.h"
#include "ios/web/public/service_names.mojom.h"
#include "ios/web/public/web_client.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/mojom/constants.mojom.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

const service_manager::Manifest& GetWebPackagedServicesManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kPackagedServicesServiceName)
          .WithDisplayName("Web")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .CanConnectToInstancesInAnyGroup(true)
                           .CanRegisterOtherServiceInstances(true)
                           .Build())
          .RequireCapability(mojom::kBrowserServiceName, "")
          .Build()
          .Amend(GetWebClient()
                     ->GetServiceManifestOverlay(
                         mojom::kPackagedServicesServiceName)
                     .value_or(service_manager::Manifest()))};

  return *manifest;
}

}  // namespace web
