// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/gpu_mojo_media_client.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "media/base/audio_decoder.h"
#include "media/base/media_switches.h"
#include "media/gpu/chromeos/mailbox_video_frame_converter.h"
#include "media/gpu/chromeos/platform_video_frame_pool.h"
#include "media/gpu/chromeos/video_decoder_pipeline.h"
#include "media/gpu/ipc/service/vda_video_decoder.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace media {

std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
    const VideoDecoderTraits& traits) {
  switch (GetPlatformDecoderImplementationType(
      *traits.gpu_workarounds, traits.gpu_preferences, traits.gpu_info)) {
    case VideoDecoderType::kVaapi: {
      auto frame_pool = std::make_unique<PlatformVideoFramePool>(
          traits.gpu_memory_buffer_factory);
      auto frame_converter = MailboxVideoFrameConverter::Create(
          base::BindRepeating(&PlatformVideoFramePool::UnwrapFrame,
                              base::Unretained(frame_pool.get())),
          traits.gpu_task_runner, traits.get_command_buffer_stub_cb);
      return VideoDecoderPipeline::Create(
          traits.task_runner, std::move(frame_pool), std::move(frame_converter),
          traits.media_log->Clone());
    }
    case VideoDecoderType::kVda: {
      return VdaVideoDecoder::Create(
          traits.task_runner, traits.gpu_task_runner, traits.media_log->Clone(),
          *traits.target_color_space, traits.gpu_preferences,
          *traits.gpu_workarounds, traits.get_command_buffer_stub_cb);
    }
    default: {
      return nullptr;
    }
  }
}

absl::optional<SupportedVideoDecoderConfigs>
GetPlatformSupportedVideoDecoderConfigs(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info,
    base::OnceCallback<SupportedVideoDecoderConfigs()> get_vda_configs) {
  VideoDecoderType decoder_implementation =
      GetPlatformDecoderImplementationType(gpu_workarounds, gpu_preferences,
                                           gpu_info);
#if BUILDFLAG(IS_LINUX)
  base::UmaHistogramEnumeration("Media.VaapiLinux.Supported",
                                decoder_implementation);
#endif
  switch (decoder_implementation) {
    case VideoDecoderType::kVda:
      return std::move(get_vda_configs).Run();
    case VideoDecoderType::kVaapi:
      return VideoDecoderPipeline::GetSupportedConfigs(gpu_workarounds);
    default:
      return absl::nullopt;
  }
}

VideoDecoderType GetPlatformDecoderImplementationType(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info) {
#if BUILDFLAG(IS_CHROMEOS)
  if (gpu_preferences.enable_chromeos_direct_video_decoder)
    return VideoDecoderType::kVaapi;
  return VideoDecoderType::kVda;
#elif BUILDFLAG(ENABLE_VULKAN)
  if (!base::FeatureList::IsEnabled(kVaapiVideoDecodeLinux))
    return VideoDecoderType::kUnknown;
  if (!base::FeatureList::IsEnabled(kUseChromeOSDirectVideoDecoder)) {
    return gpu_preferences.gr_context_type == gpu::GrContextType::kGL
               ? VideoDecoderType::kVda
               : VideoDecoderType::kUnknown;
  }
  if (gpu_preferences.gr_context_type != gpu::GrContextType::kVulkan)
    return VideoDecoderType::kUnknown;
  if (gpu_info.vulkan_info->physical_devices.empty())
    return VideoDecoderType::kUnknown;
  constexpr int kIntel = 0x8086;
  const auto& device = gpu_info.vulkan_info->physical_devices[0];
  switch (device.properties.vendorID) {
    case kIntel: {
      if (device.properties.driverVersion < VK_MAKE_VERSION(21, 1, 5))
        return VideoDecoderType::kUnknown;
      return VideoDecoderType::kVaapi;
    }
    default: {
      // NVIDIA drivers have a broken implementation of most va_* methods,
      // ARM & AMD aren't tested yet, and ImgTec/Qualcomm don't have a vaapi
      // driver.
      return VideoDecoderType::kUnknown;
    }
  }
#else
  NOTREACHED();
  return VideoDecoderType::kUnknown;
#endif
}

std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return nullptr;
}

#if !BUILDFLAG(IS_CHROMEOS)
class CdmFactory {};
#endif  // !BUILDFLAG(IS_CHROMEOS)

std::unique_ptr<CdmFactory> CreatePlatformCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
#if BUILDFLAG(IS_CHROMEOS)
  return std::make_unique<chromeos::ChromeOsCdmFactory>(frame_interfaces);
#else   // BUILDFLAG(IS_CHROMEOS)
  return nullptr;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace media
