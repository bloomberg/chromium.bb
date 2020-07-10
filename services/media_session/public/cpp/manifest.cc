// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/media_session/public/cpp/manifest.h"

#include <set>

#include "base/no_destructor.h"
#include "services/media_session/public/mojom/audio_focus.mojom.h"
#include "services/media_session/public/mojom/constants.mojom.h"
#include "services/media_session/public/mojom/media_controller.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace media_session {

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Media Session Service")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSharedAcrossGroups)
                           .Build())
          .ExposeCapability(
              "app",
              service_manager::Manifest::InterfaceList<
                  mojom::AudioFocusManager, mojom::AudioFocusManagerDebug,
                  mojom::MediaControllerManager>())
          .ExposeCapability("tests", std::set<const char*>{"*"})
          .Build()};
  return *manifest;
}

}  // namespace media_session
