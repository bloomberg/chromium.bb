// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/proxy_resolver/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "services/proxy_resolver/public/mojom/proxy_resolver.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/strings/grit/services_strings.h"

namespace proxy_resolver {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kProxyResolverServiceName)
          .WithDisplayName(IDS_PROXY_RESOLVER_DISPLAY_NAME)
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
#if defined(OS_ANDROID)
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kInProcessBuiltin)
#else
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
#endif
                  .WithInstanceSharingPolicy(
                      service_manager::Manifest::InstanceSharingPolicy::
                          kSharedAcrossGroups)
                  .Build())
          .ExposeCapability("factory", service_manager::Manifest::InterfaceList<
                                           mojom::ProxyResolverFactory>())
          .Build()};
  return *manifest;
}

}  // namespace proxy_resolver
