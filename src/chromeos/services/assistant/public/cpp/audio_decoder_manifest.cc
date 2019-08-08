// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/cpp/audio_decoder_manifest.h"

#include "base/no_destructor.h"
#include "chromeos/services/assistant/public/mojom/assistant_audio_decoder.mojom.h"
#include "chromeos/services/assistant/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace chromeos {
namespace assistant {

const service_manager::Manifest& GetAudioDecoderManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kAudioDecoderServiceName)
          .WithDisplayName("Assistant Audio Decoder Service")
          .WithOptions(
              service_manager::ManifestOptionsBuilder()
                  .WithExecutionMode(service_manager::Manifest::ExecutionMode::
                                         kOutOfProcessBuiltin)
                  .WithSandboxType("utility")
                  .WithInstanceSharingPolicy(
                      service_manager::Manifest::InstanceSharingPolicy::
                          kSharedAcrossGroups)
                  .Build())
          .ExposeCapability("assistant:audio_decoder",
                            service_manager::Manifest::InterfaceList<
                                mojom::AssistantAudioDecoderFactory>())
          .Build()};
  return *manifest;
}

}  // namespace assistant
}  // namespace chromeos
