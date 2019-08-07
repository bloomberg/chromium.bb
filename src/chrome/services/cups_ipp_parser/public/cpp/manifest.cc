// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_ipp_parser/public/cpp/manifest.h"

#include <set>

#include "base/no_destructor.h"
#include "chrome/services/cups_ipp_parser/public/mojom/constants.mojom.h"
#include "chrome/services/cups_ipp_parser/public/mojom/ipp_parser.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

const service_manager::Manifest& GetCupsIppParserManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(chrome::mojom::kCupsIppParserServiceName)
          .WithDisplayName("CUPS IPP Parser")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("utility")
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability("ipp_parser",
                            service_manager::Manifest::InterfaceList<
                                chrome::mojom::IppParser>())
          .Build()};
  return *manifest;
}
