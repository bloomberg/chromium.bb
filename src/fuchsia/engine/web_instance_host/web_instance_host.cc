// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/web_instance_host/web_instance_host.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/fdio/directory.h>
#include <lib/fdio/fd.h>
#include <lib/fdio/io.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <zircon/processargs.h>

#include <string>
#include <utility>
#include <vector>

#include "base/base_paths_fuchsia.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"
#include "content/public/common/content_switches.h"
#include "fuchsia/base/config_reader.h"
#include "fuchsia/base/feedback_registration.h"
#include "fuchsia/base/string_util.h"
#include "fuchsia/engine/features.h"
#include "fuchsia/engine/switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/key_system_names.h"
#include "media/base/media_switches.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/public/ozone_switches.h"

namespace cr_fuchsia {

namespace {

// Path to the definition file for web Component instances.
constexpr char kWebInstanceComponentPath[] = "/pkg/meta/web_instance.cmx";

// Test-only URL for web hosting Component instances with WebUI resources.
const char kWebInstanceWithWebUiComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/web_engine_with_webui#meta/web_instance.cmx";

// Use a constexpr instead of the existing base::Feature, because of the
// additional dependencies required.
constexpr char kMixedContentAutoupgradeFeatureName[] =
    "AutoupgradeMixedContent";
constexpr char kDisableMixedContentAutoupgradeOrigin[] =
    "disable-mixed-content-autoupgrade";

// Registers product data for the web_instance Component, ensuring it is
// registered regardless of how the Component is launched and without requiring
// all of its clients to provide the required services (until a better solution
// is available - see crbug.com/1211174). This should only be called once per
// process, and the calling thread must have an async_dispatcher.
void RegisterWebInstanceProductData() {
  // TODO(fxbug.dev/51490): Use a programmatic mechanism to obtain this.
  constexpr char kComponentUrl[] =
      "fuchsia-pkg://fuchsia.com/web_engine#meta/web_instance.cmx";
  constexpr char kCrashProductName[] = "FuchsiaWebEngine";
  constexpr char kFeedbackAnnotationsNamespace[] = "web-engine";

  cr_fuchsia::RegisterProductDataForCrashReporting(kComponentUrl,
                                                   kCrashProductName);

  cr_fuchsia::RegisterProductDataForFeedback(kFeedbackAnnotationsNamespace);
}

// Returns the underlying channel if |directory| is a client endpoint for a
// |fuchsia::io::Directory| protocol. Otherwise, returns an empty channel.
zx::channel ValidateDirectoryAndTakeChannel(
    fidl::InterfaceHandle<fuchsia::io::Directory> directory_handle) {
  fidl::SynchronousInterfacePtr<fuchsia::io::Directory> directory =
      directory_handle.BindSync();

  fuchsia::io::NodeInfo info;
  zx_status_t status = directory->Describe(&info);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "Describe()";
    return {};
  }

  if (!info.is_directory()) {
    DLOG(ERROR) << "Not a directory.";
    return {};
  }

  return directory.Unbind().TakeChannel();
}

void AppendFeature(base::StringPiece features_flag,
                   base::StringPiece feature_string,
                   base::CommandLine* command_line) {
  if (!command_line->HasSwitch(features_flag)) {
    command_line->AppendSwitchNative(std::string(features_flag),
                                     feature_string);
    return;
  }

  std::string new_feature_string = base::StrCat(
      {command_line->GetSwitchValueASCII(features_flag), ",", feature_string});
  command_line->RemoveSwitch(features_flag);
  command_line->AppendSwitchNative(std::string(features_flag),
                                   new_feature_string);
}

// File names must not contain directory separators, nor match the special
// current- nor parent-directory filenames.
bool IsValidContentDirectoryName(base::StringPiece file_name) {
  if (file_name.find_first_of(base::FilePath::kSeparators, 0,
                              base::FilePath::kSeparatorsLength - 1) !=
      base::StringPiece::npos) {
    return false;
  }
  if (file_name == base::FilePath::kCurrentDirectory ||
      file_name == base::FilePath::kParentDirectory) {
    return false;
  }
  return true;
}

bool HandleDataDirectoryParam(fuchsia::web::CreateContextParams* params,
                              base::CommandLine* launch_args,
                              fuchsia::sys::LaunchInfo* launch_info) {
  if (!params->has_data_directory()) {
    // Caller requested a web instance without any peristence.
    launch_args->AppendSwitch(switches::kIncognito);
    return true;
  }

  zx::channel data_directory_channel = ValidateDirectoryAndTakeChannel(
      std::move(*params->mutable_data_directory()));
  if (!data_directory_channel) {
    LOG(ERROR) << "Invalid argument |data_directory| in CreateContextParams.";
    return false;
  }

  base::FilePath data_path;
  CHECK(base::PathService::Get(base::DIR_APP_DATA, &data_path));
  launch_info->flat_namespace->paths.push_back(data_path.value());
  launch_info->flat_namespace->directories.push_back(
      std::move(data_directory_channel));
  if (params->has_data_quota_bytes()) {
    launch_args->AppendSwitchNative(
        switches::kDataQuotaBytes,
        base::NumberToString(params->data_quota_bytes()));
  }

  return true;
}

bool HandleCdmDataDirectoryParam(fuchsia::web::CreateContextParams* params,
                                 base::CommandLine* launch_args,
                                 fuchsia::sys::LaunchInfo* launch_info) {
  if (!params->has_cdm_data_directory())
    return true;

  const char kCdmDataPath[] = "/cdm_data";

  zx::channel cdm_data_directory_channel = ValidateDirectoryAndTakeChannel(
      std::move(*params->mutable_cdm_data_directory()));
  if (!cdm_data_directory_channel) {
    LOG(ERROR)
        << "Invalid argument |cdm_data_directory| in CreateContextParams.";
    return false;
  }

  launch_args->AppendSwitchNative(switches::kCdmDataDirectory, kCdmDataPath);
  launch_info->flat_namespace->paths.push_back(kCdmDataPath);
  launch_info->flat_namespace->directories.push_back(
      std::move(cdm_data_directory_channel));
  if (params->has_cdm_data_quota_bytes()) {
    launch_args->AppendSwitchNative(
        switches::kCdmDataQuotaBytes,
        base::NumberToString(params->cdm_data_quota_bytes()));
  }

  return true;
}

bool HandleUserAgentParams(fuchsia::web::CreateContextParams* params,
                           base::CommandLine* launch_args) {
  if (!params->has_user_agent_product()) {
    if (params->has_user_agent_version()) {
      LOG(ERROR) << "Embedder version without product.";
      return false;
    }
    return true;
  }

  if (!net::HttpUtil::IsToken(params->user_agent_product())) {
    LOG(ERROR) << "Invalid embedder product.";
    return false;
  }

  std::string product_and_version(params->user_agent_product());
  if (params->has_user_agent_version()) {
    if (!net::HttpUtil::IsToken(params->user_agent_version())) {
      LOG(ERROR) << "Invalid embedder version.";
      return false;
    }
    base::StrAppend(&product_and_version, {"/", params->user_agent_version()});
  }
  launch_args->AppendSwitchNative(switches::kUserAgentProductAndVersion,
                                  std::move(product_and_version));
  return true;
}

void HandleUnsafelyTreatInsecureOriginsAsSecureParam(
    fuchsia::web::CreateContextParams* params,
    base::CommandLine* launch_args) {
  if (!params->has_unsafely_treat_insecure_origins_as_secure())
    return;

  const std::vector<std::string>& insecure_origins =
      params->unsafely_treat_insecure_origins_as_secure();
  for (auto origin : insecure_origins) {
    if (origin == switches::kAllowRunningInsecureContent) {
      launch_args->AppendSwitch(switches::kAllowRunningInsecureContent);
    } else if (origin == kDisableMixedContentAutoupgradeOrigin) {
      AppendFeature(switches::kDisableFeatures,
                    kMixedContentAutoupgradeFeatureName, launch_args);
    } else {
      // Pass the rest of the list to the Context process.
      AppendFeature(network::switches::kUnsafelyTreatInsecureOriginAsSecure,
                    origin, launch_args);
    }
  }
}

void HandleCorsExemptHeadersParam(fuchsia::web::CreateContextParams* params,
                                  base::CommandLine* launch_args) {
  if (!params->has_cors_exempt_headers())
    return;

  std::vector<base::StringPiece> cors_exempt_headers;
  cors_exempt_headers.reserve(params->cors_exempt_headers().size());
  for (const auto& header : params->cors_exempt_headers()) {
    cors_exempt_headers.push_back(cr_fuchsia::BytesAsString(header));
  }

  launch_args->AppendSwitchNative(switches::kCorsExemptHeaders,
                                  base::JoinString(cors_exempt_headers, ","));
}

bool HandleContentDirectoriesParam(fuchsia::web::CreateContextParams* params,
                                   base::CommandLine* launch_args,
                                   fuchsia::sys::LaunchInfo* launch_info) {
  DCHECK(launch_info);
  DCHECK(launch_info->flat_namespace);

  if (!params->has_content_directories())
    return true;

  auto* directories = params->mutable_content_directories();
  for (size_t i = 0; i < directories->size(); ++i) {
    fuchsia::web::ContentDirectoryProvider& directory = directories->at(i);

    if (!IsValidContentDirectoryName(directory.name())) {
      DLOG(ERROR) << "Invalid directory name: " << directory.name();
      return false;
    }

    zx::channel validated_channel = ValidateDirectoryAndTakeChannel(
        std::move(*directory.mutable_directory()));
    if (!validated_channel) {
      DLOG(ERROR) << "Service directory handle not valid for directory: "
                  << directory.name();
      return false;
    }

    const base::FilePath kContentDirectories("/content-directories");
    launch_info->flat_namespace->paths.push_back(
        kContentDirectories.Append(directory.name()).value());
    launch_info->flat_namespace->directories.push_back(
        std::move(validated_channel));
  }

  launch_args->AppendSwitch(switches::kEnableContentDirectories);

  return true;
}

// Returns false if the config is present but has invalid contents.
bool MaybeAddCommandLineArgsFromConfig(const base::Value& config,
                                       base::CommandLine* command_line) {
  const base::Value* args = config.FindDictKey("command-line-args");
  if (!args)
    return true;

  static const base::StringPiece kAllowedArgs[] = {
      blink::switches::kGpuRasterizationMSAASampleCount,
      blink::switches::kMinHeightForGpuRasterTile,
      cc::switches::kEnableClippedImageScaling,
      cc::switches::kEnableGpuBenchmarking,
      switches::kDisableFeatures,
      switches::kDisableGpuWatchdog,
      switches::kDisableMipmapGeneration,
      // TODO(crbug.com/1082821): Remove this switch from the allow-list.
      switches::kEnableCastStreamingReceiver,
      switches::kEnableFeatures,
      switches::kEnableLowEndDeviceMode,
      switches::kForceGpuMemAvailableMb,
      switches::kForceGpuMemDiscardableLimitMb,
      switches::kForceMaxTextureSize,
      switches::kGoogleApiKey,
      switches::kMaxDecodedImageSizeMb,
      switches::kRendererProcessLimit,
      switches::kUseCmdDecoder,
      switches::kV,
      switches::kVModule,
      switches::kVulkanHeapMemoryLimitMb,
      switches::kVulkanSyncCpuMemoryLimitMb,
      switches::kWebglAntialiasingMode,
      switches::kWebglMSAASampleCount,
  };

  for (const auto& arg : args->DictItems()) {
    if (!base::Contains(kAllowedArgs, arg.first)) {
      // TODO(https://crbug.com/1032439): Increase severity and return false
      // once we have a mechanism for soft transitions of supported arguments.
      LOG(WARNING) << "Unknown command-line arg: '" << arg.first
                   << "'. Config file and WebEngine version may not match.";
      continue;
    }

    DCHECK(!command_line->HasSwitch(arg.first));
    if (arg.second.is_none()) {
      command_line->AppendSwitch(arg.first);
    } else if (arg.second.is_string()) {
      command_line->AppendSwitchNative(arg.first, arg.second.GetString());
    } else {
      LOG(ERROR) << "Config command-line arg must be a string: " << arg.first;
      return false;
    }

    // TODO(https://crbug.com/1023012): enable-low-end-device-mode currently
    // fakes 512MB total physical memory, which triggers RGB4444 textures,
    // which
    // we don't yet support.
    if (arg.first == switches::kEnableLowEndDeviceMode)
      command_line->AppendSwitch(blink::switches::kDisableRGBA4444Textures);
  }

  return true;
}

// Returns true if DRM is supported in current configuration. Currently we
// assume that it is supported on ARM64, but not on x64.
//
// TODO(crbug.com/1013412): Detect support for all features required for
// FuchsiaCdm. Specifically we need to verify that protected memory is supported
// and that mediacodec API provides hardware video decoders.
bool IsFuchsiaCdmSupported() {
#if defined(ARCH_CPU_ARM64)
  return true;
#else
  return false;
#endif
}

std::vector<std::string> LoadWebInstanceSandboxServices() {
  std::string cmx;
  CHECK(ReadFileToString(base::FilePath(kWebInstanceComponentPath), &cmx));
  absl::optional<base::Value> json = base::JSONReader::Read(cmx);
  const base::Value* services = json->FindListPath("sandbox.services");
  std::vector<std::string> result;
  for (auto& entry : services->GetList())
    result.push_back(std::move(entry.GetString()));
  return result;
}

}  // namespace

// Production URL for web hosting Component instances.
const char WebInstanceHost::kComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/web_engine#meta/web_instance.cmx";

WebInstanceHost::WebInstanceHost() {
  // Ensure WebInstance is registered before launching it.
  // TODO(crbug.com/1211174): Replace with a different mechanism when available.
  RegisterWebInstanceProductData();
}

WebInstanceHost::~WebInstanceHost() = default;

zx_status_t WebInstanceHost::CreateInstanceForContext(
    fuchsia::web::CreateContextParams params,
    fidl::InterfaceRequest<fuchsia::io::Directory> services_request) {
  DCHECK(services_request);

  if (!params.has_service_directory()) {
    DLOG(ERROR)
        << "Missing argument |service_directory| in CreateContextParams.";
    return ZX_ERR_INVALID_ARGS;
  }

  fidl::InterfaceHandle<::fuchsia::io::Directory> service_directory =
      std::move(*params.mutable_service_directory());
  if (!service_directory) {
    DLOG(ERROR) << "Invalid |service_directory| in CreateContextParams.";
    return ZX_ERR_INVALID_ARGS;
  }

  // Base the new Component's command-line on this process' command-line.
  base::CommandLine launch_args(*base::CommandLine::ForCurrentProcess());
  launch_args.RemoveSwitch(switches::kContextProvider);

  fuchsia::sys::LaunchInfo launch_info;
  // TODO(1010222): Make kComponentUrl a relative component URL, and
  // remove this workaround.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("with-webui"))
    launch_info.url = kWebInstanceWithWebUiComponentUrl;
  else
    launch_info.url = kComponentUrl;
  launch_info.flat_namespace = fuchsia::sys::FlatNamespace::New();

  // Process command-line settings specified in our package config-data.
  base::Value web_engine_config;
  if (config_for_test_.is_none()) {
    const absl::optional<base::Value>& config = cr_fuchsia::LoadPackageConfig();
    web_engine_config =
        config ? config->Clone() : base::Value(base::Value::Type::DICTIONARY);
  } else {
    web_engine_config = std::move(config_for_test_);
  }
  if (!MaybeAddCommandLineArgsFromConfig(web_engine_config, &launch_args)) {
    return ZX_ERR_INTERNAL;
  }

  if (params.has_remote_debugging_port()) {
    launch_args.AppendSwitchNative(
        switches::kRemoteDebuggingPort,
        base::NumberToString(params.remote_debugging_port()));
  }

  fuchsia::web::ContextFeatureFlags features = {};
  if (params.has_features())
    features = params.features();

  const bool is_headless =
      (features & fuchsia::web::ContextFeatureFlags::HEADLESS) ==
      fuchsia::web::ContextFeatureFlags::HEADLESS;
  if (is_headless) {
    launch_args.AppendSwitchNative(switches::kOzonePlatform,
                                   switches::kHeadless);
    launch_args.AppendSwitch(switches::kHeadless);
  }

  if ((features & fuchsia::web::ContextFeatureFlags::LEGACYMETRICS) ==
      fuchsia::web::ContextFeatureFlags::LEGACYMETRICS) {
    launch_args.AppendSwitch(switches::kUseLegacyMetricsService);
  }

  const bool enable_vulkan =
      (features & fuchsia::web::ContextFeatureFlags::VULKAN) ==
      fuchsia::web::ContextFeatureFlags::VULKAN;
  bool enable_widevine =
      (features & fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM) ==
      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM;
  bool enable_playready = params.has_playready_key_system();

  // Verify that the configuration is compatible with DRM, if requested.
  if (enable_widevine || enable_playready) {
    // VULKAN is required for DRM-protected video playback. Allow DRM to also be
    // enabled for HEADLESS Contexts, since Vulkan is never required for audio.
    if (!enable_vulkan && !is_headless) {
      DLOG(ERROR) << "WIDEVINE_CDM and PLAYREADY_CDM features require VULKAN "
                     " or HEADLESS.";
      return ZX_ERR_NOT_SUPPORTED;
    }
    if (!params.has_cdm_data_directory()) {
      LOG(ERROR) << "WIDEVINE_CDM and PLAYREADY_CDM features require a "
                    "|cdm_data_directory|.";
      return ZX_ERR_NOT_SUPPORTED;
    }
    // |cdm_data_directory| will be handled later.
  }

  // If the system doesn't actually support DRM then disable it. This may result
  // in the Context being able to run without using protected buffers.
  if (enable_playready && !IsFuchsiaCdmSupported()) {
    LOG(WARNING) << "PlayReady is not supported on this device.";
    enable_playready = false;
  }
  if (enable_widevine && !IsFuchsiaCdmSupported()) {
    LOG(WARNING) << "Widevine is not supported on this device.";
    enable_widevine = false;
  }

  bool allow_protected_graphics =
      web_engine_config.FindBoolPath("allow-protected-graphics")
          .value_or(false);
  bool force_protected_graphics =
      web_engine_config.FindBoolPath("force-protected-graphics")
          .value_or(false);
  bool enable_protected_graphics =
      ((enable_playready || enable_widevine) && allow_protected_graphics) ||
      force_protected_graphics;
  bool use_overlays_for_video =
      web_engine_config.FindBoolPath("use-overlays-for-video").value_or(false);

  if (enable_protected_graphics) {
    launch_args.AppendSwitch(switches::kEnableVulkanProtectedMemory);
    launch_args.AppendSwitch(switches::kEnableProtectedVideoBuffers);
    bool force_protected_video_buffers =
        web_engine_config.FindBoolPath("force-protected-video-buffers")
            .value_or(false);
    if (force_protected_video_buffers) {
      launch_args.AppendSwitch(switches::kForceProtectedVideoOutputBuffers);
    }
  }

  if (use_overlays_for_video) {
    // Overlays are only available if OutputPresenterFuchsia is in use.
    AppendFeature(switches::kEnableFeatures,
                  features::kUseSkiaOutputDeviceBufferQueue.name, &launch_args);
    AppendFeature(switches::kEnableFeatures,
                  features::kUseRealBuffersForPageFlipTest.name, &launch_args);
    launch_args.AppendSwitchASCII(switches::kEnableHardwareOverlays,
                                  "underlay");
    launch_args.AppendSwitch(switches::kUseOverlaysForVideo);
  }

  if (enable_vulkan) {
    if (is_headless) {
      DLOG(ERROR) << "VULKAN and HEADLESS features cannot be used together.";
      return ZX_ERR_NOT_SUPPORTED;
    }

    VLOG(1) << "Enabling Vulkan GPU acceleration.";
    // Vulkan requires use of SkiaRenderer, configured to a use Vulkan context.
    launch_args.AppendSwitch(switches::kUseVulkan);
    const std::vector<base::StringPiece> enabled_features = {
        features::kUseSkiaRenderer.name, features::kVulkan.name};
    AppendFeature(switches::kEnableFeatures,
                  base::JoinString(enabled_features, ","), &launch_args);

    // SkiaRenderer requires out-of-process rasterization be enabled.
    launch_args.AppendSwitch(switches::kEnableOopRasterization);

    launch_args.AppendSwitchASCII(switches::kUseGL,
                                  gl::kGLImplementationANGLEName);
  } else {
    VLOG(1) << "Disabling GPU acceleration.";
    // Disable use of Vulkan GPU, and use of the software-GL rasterizer. The
    // Context will still run a GPU process, but will not support WebGL.
    launch_args.AppendSwitch(switches::kDisableGpu);
    launch_args.AppendSwitch(switches::kDisableSoftwareRasterizer);
  }

  if (enable_widevine) {
    launch_args.AppendSwitch(switches::kEnableWidevine);
  }

  if (enable_playready) {
    const std::string& key_system = params.playready_key_system();
    if (key_system == kWidevineKeySystem || media::IsClearKey(key_system)) {
      LOG(ERROR)
          << "Invalid value for CreateContextParams/playready_key_system: "
          << key_system;
      return ZX_ERR_INVALID_ARGS;
    }
    launch_args.AppendSwitchNative(switches::kPlayreadyKeySystem, key_system);
  }

  bool enable_audio = (features & fuchsia::web::ContextFeatureFlags::AUDIO) ==
                      fuchsia::web::ContextFeatureFlags::AUDIO;
  if (!enable_audio) {
    // TODO(fxbug.dev/58902): Split up audio input and output in
    // ContextFeatureFlags.
    launch_args.AppendSwitch(switches::kDisableAudioOutput);
    launch_args.AppendSwitch(switches::kDisableAudioInput);
  }

  bool enable_hardware_video_decoder =
      (features & fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER) ==
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER;
  if (!enable_hardware_video_decoder)
    launch_args.AppendSwitch(switches::kDisableAcceleratedVideoDecode);

  if (enable_hardware_video_decoder && !enable_vulkan) {
    DLOG(ERROR) << "HARDWARE_VIDEO_DECODER requires VULKAN.";
    return ZX_ERR_NOT_SUPPORTED;
  }

  bool disable_software_video_decoder =
      (features &
       fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY) ==
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY;
  if (disable_software_video_decoder) {
    if (!enable_hardware_video_decoder) {
      LOG(ERROR) << "Software video decoding may only be disabled if hardware "
                    "video decoding is enabled.";
      return ZX_ERR_INVALID_ARGS;
    }

    AppendFeature(switches::kDisableFeatures,
                  features::kEnableSoftwareOnlyVideoCodecs.name, &launch_args);
  }

  if (!HandleCdmDataDirectoryParam(&params, &launch_args, &launch_info)) {
    return ZX_ERR_INVALID_ARGS;
  }
  if (!HandleDataDirectoryParam(&params, &launch_args, &launch_info)) {
    return ZX_ERR_INVALID_ARGS;
  }
  if (!HandleContentDirectoriesParam(&params, &launch_args, &launch_info)) {
    return ZX_ERR_INVALID_ARGS;
  }
  if (!HandleUserAgentParams(&params, &launch_args)) {
    return ZX_ERR_INVALID_ARGS;
  }
  HandleUnsafelyTreatInsecureOriginsAsSecureParam(&params, &launch_args);
  HandleCorsExemptHeadersParam(&params, &launch_args);

  // Set the command-line flag to enable DevTools, if requested.
  if (enable_remote_debug_mode_)
    launch_args.AppendSwitch(switches::kEnableRemoteDebugMode);

  // In tests the ContextProvider is configured to log to stderr, so clone the
  // handle to allow web instances to also log there.
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          "enable-logging") == "stderr") {
    launch_info.err = fuchsia::sys::FileDescriptor::New();
    launch_info.err->type0 = PA_FD;
    zx_status_t status = fdio_fd_clone(
        STDERR_FILENO, launch_info.err->handle0.reset_and_get_address());
    ZX_CHECK(status == ZX_OK, status);
  }

  // Pass on the caller's service-directory request.
  launch_info.directory_request = services_request.TakeChannel();

  // Set |additional_services| to redirect requests for all services specified
  // in the web instance component manifest to be satisfied by the caller-
  // supplied service directory. This ensures that the instance cannot access
  // any services outside those provided by the caller.
  launch_info.additional_services = fuchsia::sys::ServiceList::New();
  launch_info.additional_services->names = LoadWebInstanceSandboxServices();
  launch_info.additional_services->host_directory =
      service_directory.TakeChannel();

  // Take the accumulated command line arguments, omitting the program name in
  // argv[0], and set them in |launch_info|.
  launch_info.arguments = std::vector<std::string>(
      launch_args.argv().begin() + 1, launch_args.argv().end());

  // Launch the component with the accumulated settings.  The Component will
  // self-terminate when the fuchsia.web.Context client disconnects.
  IsolatedEnvironmentLauncher()->CreateComponent(std::move(launch_info),
                                                 nullptr /* controller */);

  return ZX_OK;
}

fuchsia::sys::Launcher* WebInstanceHost::IsolatedEnvironmentLauncher() {
  if (isolated_environment_launcher_)
    return isolated_environment_launcher_.get();

  // Create the nested isolated Environment. This environment provides only the
  // fuchsia.sys.Loader service, which is required to allow the Launcher to
  // resolve the web instance package. All other services are provided
  // explicitly to each web instance, from those passed to |CreateContext()|.
  auto environment = base::ComponentContextForProcess()
                         ->svc()
                         ->Connect<fuchsia::sys::Environment>();

  // Populate a ServiceList providing only the Loader service.
  auto services = fuchsia::sys::ServiceList::New();
  services->names.push_back(fuchsia::sys::Loader::Name_);
  fidl::InterfaceHandle<::fuchsia::io::Directory> services_channel;
  environment->GetDirectory(services_channel.NewRequest().TakeChannel());
  services->host_directory = services_channel.TakeChannel();

  // Instantiate the isolated environment. This ContextProvider instance's PID
  // is included in the label to ensure that concurrent service instances
  // launched in the same Environment (e.g. during tests) do not clash.
  fuchsia::sys::EnvironmentPtr isolated_environment;
  environment->CreateNestedEnvironment(
      isolated_environment.NewRequest(),
      isolated_environment_controller_.NewRequest(),
      base::StringPrintf("web_instances:%lu", base::Process::Current().Pid()),
      std::move(services),
      {.inherit_parent_services = false,
       .use_parent_runners = false,
       .delete_storage_on_death = true});

  // The ContextProvider only needs to retain the EnvironmentController and
  // a connection to the Launcher service for the isolated environment.
  isolated_environment->GetLauncher(
      isolated_environment_launcher_.NewRequest());
  isolated_environment_launcher_.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "Launcher disconnected.";
  });
  isolated_environment_controller_.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "EnvironmentController disconnected.";
  });

  return isolated_environment_launcher_.get();
}

}  // namespace cr_fuchsia
