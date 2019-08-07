// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/manifest.h"

#include "base/no_destructor.h"
#include "services/metrics/public/mojom/constants.mojom.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace metrics {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kMetricsServiceName)
          .WithDisplayName("Metrics Service")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability("url_keyed_metrics",
                            service_manager::Manifest::InterfaceList<
                                ukm::mojom::UkmRecorderInterface>())
          .Build()};
  return *manifest;
}

}  // namespace metrics
