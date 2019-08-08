// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/app/content_packaged_services_manifest.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/services/heap_profiling/public/cpp/manifest.h"
#include "content/public/common/service_names.mojom.h"
#include "media/mojo/services/cdm_manifest.h"
#include "media/mojo/services/media_manifest.h"
#include "services/audio/public/cpp/manifest.h"
#include "services/data_decoder/public/cpp/manifest.h"
#include "services/device/public/cpp/manifest.h"
#include "services/media_session/public/cpp/manifest.h"
#include "services/metrics/public/cpp/manifest.h"
#include "services/network/public/cpp/manifest.h"
#include "services/resource_coordinator/public/cpp/manifest.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/shape_detection/public/cpp/manifest.h"
#include "services/tracing/manifest.h"
#include "services/video_capture/public/cpp/manifest.h"
#include "services/viz/public/cpp/manifest.h"

#if defined(OS_LINUX)
#include "components/services/font/public/cpp/manifest.h"  // nogncheck
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/assistant/buildflags.h"
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#include "chromeos/services/assistant/public/cpp/audio_decoder_manifest.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // defined(OS_CHROMEOS)

namespace content {

const service_manager::Manifest& GetContentPackagedServicesManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest {
    service_manager::ManifestBuilder()
        .WithServiceName(mojom::kPackagedServicesServiceName)
        .WithOptions(service_manager::ManifestOptionsBuilder()
                         .WithInstanceSharingPolicy(
                             service_manager::Manifest::InstanceSharingPolicy::
                                 kSingleton)
                         .CanConnectToInstancesInAnyGroup(true)
                         .CanRegisterOtherServiceInstances(true)
                         .Build())
        .RequireCapability(mojom::kBrowserServiceName, "")
        .RequireCapability("*", "app")
        .PackageService(heap_profiling::GetManifest())
        .PackageService(media::GetCdmManifest())
        .PackageService(media::GetMediaManifest())
        .PackageService(audio::GetManifest())
        .PackageService(data_decoder::GetManifest())
        .PackageService(device::GetManifest())
        .PackageService(media_session::GetManifest())
        .PackageService(metrics::GetManifest())
        .PackageService(network::GetManifest())
        .PackageService(resource_coordinator::GetManifest())
        .PackageService(shape_detection::GetManifest())
        .PackageService(tracing::GetManifest())
        .PackageService(video_capture::GetManifest())
        .PackageService(viz::GetManifest())
#if defined(OS_LINUX)
        .PackageService(font_service::GetManifest())
#endif
#if defined(OS_CHROMEOS)
#if BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
        // TODO(https://crbug.com/929340): This doesn't belong here!
        .PackageService(chromeos::assistant::GetAudioDecoderManifest())
#endif  // BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
#endif  // defined(OS_CHROMEOS)
        .Build()
  };
  return *manifest;
}

}  // namespace content
