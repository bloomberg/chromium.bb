// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/context_provider_impl.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/async/default.h>
#include <lib/fdio/directory.h>
#include <lib/fdio/fd.h>
#include <lib/fdio/io.h>
#include <lib/zx/job.h>
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
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/fuchsia/default_job.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "content/public/common/content_switches.h"
#include "fuchsia/engine/common/web_engine_content_client.h"
#include "fuchsia/engine/switches.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "media/base/key_system_names.h"
#include "media/base/media_switches.h"
#include "net/http/http_util.h"
#include "services/service_manager/sandbox/fuchsia/sandbox_policy_fuchsia.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"
#include "ui/gfx/switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/ozone/public/ozone_switches.h"

namespace {

// Returns the underlying channel if |directory| is a client endpoint for a
// |fuchsia::io::Directory| protocol. Otherwise, returns an empty channel.
zx::channel ValidateDirectoryAndTakeChannel(
    fidl::InterfaceHandle<fuchsia::io::Directory> directory_handle) {
  fidl::SynchronousInterfacePtr<fuchsia::io::Directory> directory =
      directory_handle.BindSync();
  zx_status_t status = ZX_ERR_INTERNAL;
  std::vector<uint8_t> entries;

  directory->ReadDirents(0, &status, &entries);
  if (status == ZX_OK) {
    return directory.Unbind().TakeChannel();
  }

  // Not a directory.
  return zx::channel();
}

// Populates a CommandLine with content directory name/handle pairs.
bool SetContentDirectoriesInCommandLine(
    std::vector<fuchsia::web::ContentDirectoryProvider> directories,
    base::CommandLine* command_line,
    base::LaunchOptions* launch_options) {
  DCHECK(command_line);
  DCHECK(launch_options);

  std::vector<std::string> directory_pairs;
  for (size_t i = 0; i < directories.size(); ++i) {
    fuchsia::web::ContentDirectoryProvider& directory = directories[i];

    if (directory.name().find('=') != std::string::npos ||
        directory.name().find(',') != std::string::npos) {
      DLOG(ERROR) << "Invalid character in directory name: "
                  << directory.name();
      return false;
    }

    if (!directory.directory().is_valid()) {
      DLOG(ERROR) << "Service directory handle not valid for directory: "
                  << directory.name();
      return false;
    }

    uint32_t directory_handle_id = base::LaunchOptions::AddHandleToTransfer(
        &launch_options->handles_to_transfer,
        directory.mutable_directory()->TakeChannel().release());
    directory_pairs.emplace_back(
        base::StrCat({directory.name().c_str(), "=",
                      base::NumberToString(directory_handle_id)}));
  }

  command_line->AppendSwitchASCII(switches::kContentDirectories,
                                  base::JoinString(directory_pairs, ","));

  return true;
}

constexpr char kConfigFileName[] = "/config/data/config.json";

base::Value LoadConfigFrom(const base::FilePath& file_path) {
  if (!base::PathExists(file_path)) {
    DLOG(WARNING) << file_path.value()
                  << " doesn't exist. Using default WebEngine configuration.";
    return base::Value(base::Value::Type::DICTIONARY);
  }

  std::string file_content;
  bool loaded = base::ReadFileToString(file_path, &file_content);
  CHECK(loaded) << "Failed to read " << file_path.value();

  base::JSONReader reader;
  base::Optional<base::Value> parsed = reader.Read(file_content);
  CHECK(parsed) << "Failed to parse " << file_path.value() << ": "
                << reader.GetErrorMessage();
  CHECK(parsed->is_dict()) << "Config is not a JSON dictinary: "
                           << file_path.value();

  return std::move(parsed.value());
}

const base::Value& GetWebEngineConfig() {
  static base::Value config = LoadConfigFrom(base::FilePath(kConfigFileName));
  return config;
}

// Returns false if the config is present but has invalid contents.
bool MaybeAddCommandLineArgsFromConfig(const base::Value& config,
                                       base::CommandLine* command_line) {
  const base::Value* args = config.FindDictKey("command-line-args");
  if (!args)
    return true;

  static const base::StringPiece kAllowedArgs[] = {
      switches::kEnableFeatures,
      switches::kEnableFuchsiaAudioConsumer,
      switches::kEnableLowEndDeviceMode,
      switches::kForceGpuMemAvailableMb,
      switches::kMinHeightForGpuRasterTile,
      switches::kRendererProcessLimit,
  };

  for (const auto& arg : args->DictItems()) {
    if (!base::Contains(kAllowedArgs, arg.first)) {
      LOG(ERROR) << "Unknown command-line arg: " << arg.first;
      return false;
    }
    if (!arg.second.is_string()) {
      LOG(ERROR) << "Config command-line arg must be a string: " << arg.first;
      return false;
    }
    command_line->AppendSwitchNative(arg.first, arg.second.GetString());
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

}  // namespace

const uint32_t ContextProviderImpl::kContextRequestHandleId =
    PA_HND(PA_USER0, 0);

ContextProviderImpl::ContextProviderImpl() = default;

ContextProviderImpl::~ContextProviderImpl() = default;

void ContextProviderImpl::Create(
    fuchsia::web::CreateContextParams params,
    fidl::InterfaceRequest<fuchsia::web::Context> context_request) {
  if (!context_request.is_valid()) {
    DLOG(ERROR) << "Invalid |context_request|.";
    return;
  }
  if (!params.has_service_directory()) {
    DLOG(ERROR)
        << "Missing argument |service_directory| in CreateContextParams.";
    context_request.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  fidl::InterfaceHandle<::fuchsia::io::Directory> service_directory =
      std::move(*params.mutable_service_directory());
  if (!service_directory) {
    DLOG(WARNING) << "Invalid |service_directory| in CreateContextParams.";
    context_request.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  base::LaunchOptions launch_options;
  launch_options.process_name_suffix = ":context";

  service_manager::SandboxPolicyFuchsia sandbox_policy;
  sandbox_policy.Initialize(service_manager::SANDBOX_TYPE_WEB_CONTEXT);
  sandbox_policy.SetServiceDirectory(std::move(service_directory));
  sandbox_policy.UpdateLaunchOptionsForSandbox(&launch_options);

  // Transfer the ContextRequest handle to a well-known location in the child
  // process' handle table.
  launch_options.handles_to_transfer.push_back(
      {kContextRequestHandleId, context_request.channel().get()});

  // Bind |data_directory| to /data directory, if provided.
  if (params.has_data_directory()) {
    zx::channel data_directory_channel = ValidateDirectoryAndTakeChannel(
        std::move(*params.mutable_data_directory()));
    if (data_directory_channel.get() == ZX_HANDLE_INVALID) {
      DLOG(ERROR)
          << "Invalid argument |data_directory| in CreateContextParams.";
      context_request.Close(ZX_ERR_INVALID_ARGS);
      return;
    }

    base::FilePath data_path;
    if (!base::PathService::Get(base::DIR_APP_DATA, &data_path)) {
      DLOG(ERROR) << "Failed to get data directory service path.";
      return;
    }
    launch_options.paths_to_transfer.push_back(
        base::PathToTransfer{data_path, data_directory_channel.release()});
  }

  // Isolate the child Context processes by containing them within their own
  // respective jobs.
  zx::job job;
  zx_status_t status = zx::job::create(*base::GetDefaultJob(), 0, &job);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_job_create";
    return;
  }
  launch_options.job_handle = job.get();

  base::CommandLine launch_command = *base::CommandLine::ForCurrentProcess();
  std::vector<zx::channel> devtools_listener_channels;

  if (params.has_remote_debugging_port()) {
    launch_command.AppendSwitchNative(
        switches::kRemoteDebuggingPort,
        base::NumberToString(params.remote_debugging_port()));
  }

  if (devtools_listeners_.size() != 0) {
    // Connect DevTools listeners to the new Context process.
    std::vector<std::string> handles_ids;
    for (auto& devtools_listener : devtools_listeners_.ptrs()) {
      fidl::InterfaceHandle<fuchsia::web::DevToolsPerContextListener>
          client_listener;
      devtools_listener.get()->get()->OnContextDevToolsAvailable(
          client_listener.NewRequest());
      devtools_listener_channels.emplace_back(client_listener.TakeChannel());
      handles_ids.push_back(
          base::NumberToString(base::LaunchOptions::AddHandleToTransfer(
              &launch_options.handles_to_transfer,
              devtools_listener_channels.back().get())));
    }
    launch_command.AppendSwitchNative(switches::kRemoteDebuggerHandles,
                                      base::JoinString(handles_ids, ","));
  }

  fuchsia::web::ContextFeatureFlags features = {};
  if (params.has_features())
    features = params.features();

  bool enable_vulkan = (features & fuchsia::web::ContextFeatureFlags::VULKAN) ==
                       fuchsia::web::ContextFeatureFlags::VULKAN;
  bool enable_widevine =
      (features & fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM) ==
      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM;
  if (enable_widevine && !IsFuchsiaCdmSupported()) {
    LOG(WARNING) << "Widevine is not supported on this device.";
    enable_widevine = false;
  }

  bool enable_playready = params.has_playready_key_system();
  if (enable_playready && !IsFuchsiaCdmSupported()) {
    LOG(WARNING) << "PlayReady is not supported on this device.";
    enable_playready = false;
  }

  bool enable_drm = enable_widevine || enable_playready;
  if (enable_drm && !enable_vulkan) {
    DLOG(ERROR) << "WIDEVINE_CDM and PLAYREADY_CDM features require VULKAN.";
    context_request.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  const bool is_headless =
      (features & fuchsia::web::ContextFeatureFlags::HEADLESS) ==
      fuchsia::web::ContextFeatureFlags::HEADLESS;
  if (is_headless) {
    launch_command.AppendSwitchNative(switches::kOzonePlatform,
                                      switches::kHeadless);
    launch_command.AppendSwitch(switches::kHeadless);
  }

  if (enable_vulkan) {
    if (is_headless) {
      LOG(ERROR) << "VULKAN and HEADLESS features cannot be used together.";
      context_request.Close(ZX_ERR_INVALID_ARGS);
      return;
    }

    DLOG(ERROR) << "Enabling Vulkan GPU acceleration.";
    // Vulkan requires use of SkiaRenderer, configured to a use Vulkan context.
    launch_command.AppendSwitch(switches::kUseVulkan);
    launch_command.AppendSwitchASCII(switches::kEnableFeatures,
                                     features::kUseSkiaRenderer.name);
    launch_command.AppendSwitchASCII(switches::kGrContextType,
                                     switches::kGrContextTypeVulkan);

    // SkiaRenderer requires out-of-process rasterization be enabled.
    launch_command.AppendSwitch(switches::kEnableOopRasterization);

    // TODO(https://crbug.com/766360): Provide a no-op GL implementation until
    // vANGLE is available.
    launch_command.AppendSwitchASCII(switches::kUseGL,
                                     gl::kGLImplementationStubName);
  } else {
    DLOG(ERROR) << "Disabling GPU acceleration.";
    // Disable use of Vulkan GPU, and use of the software-GL rasterizer. The
    // Context will still run a GPU process, but will not support WebGL.
    launch_command.AppendSwitch(switches::kDisableGpu);
    launch_command.AppendSwitch(switches::kDisableSoftwareRasterizer);
  }

  const base::Value& web_engine_config =
      config_for_test_.is_none() ? GetWebEngineConfig() : config_for_test_;
  bool allow_protected_graphics =
      web_engine_config.FindBoolPath("allow-protected-graphics")
          .value_or(false);
  bool force_protected_graphics =
      web_engine_config.FindBoolPath("force-protected-graphics")
          .value_or(false);
  bool enable_protected_graphics =
      (enable_drm && allow_protected_graphics) || force_protected_graphics;

  if (enable_protected_graphics) {
    launch_command.AppendSwitch(switches::kEnforceVulkanProtectedMemory);
    launch_command.AppendSwitch(switches::kEnableProtectedVideoBuffers);
    bool force_protected_video_buffers =
        web_engine_config.FindBoolPath("force-protected-video-buffers")
            .value_or(false);
    if (force_protected_video_buffers) {
      launch_command.AppendSwitch(switches::kForceProtectedVideoOutputBuffers);
      // TODO(crbug.com/1019212): We observed flicker and buffer issues when
      // using accelerated canvas with protected memory.
      launch_command.AppendSwitch(switches::kDisableAccelerated2dCanvas);
    }
  }

  if (enable_widevine) {
    launch_command.AppendSwitch(switches::kEnableWidevine);
  }

  if (enable_playready) {
    const std::string& key_system = params.playready_key_system();
    if (key_system == kWidevineKeySystem || media::IsClearKey(key_system)) {
      DLOG(ERROR)
          << "Invalid value for CreateContextParams/playready_key_system: "
          << key_system;
      context_request.Close(ZX_ERR_INVALID_ARGS);
      return;
    }
    launch_command.AppendSwitchNative(switches::kPlayreadyKeySystem,
                                      key_system);
  }

  bool disable_software_video_decoder =
      (features &
       fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY) ==
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY;
  bool enable_hardware_video_decoder =
      (features & fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER) ==
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER;
  if (disable_software_video_decoder) {
    if (!enable_hardware_video_decoder) {
      LOG(ERROR) << "Software video decoding may only be disabled if hardware "
                    "video decoding is enabled.";
      context_request.Close(ZX_ERR_INVALID_ARGS);
      return;
    }

    launch_command.AppendSwitch(switches::kDisableSoftwareVideoDecoders);
  }

  if (!MaybeAddCommandLineArgsFromConfig(web_engine_config, &launch_command)) {
    context_request.Close(ZX_ERR_INTERNAL);
    return;
  }

  // Validate embedder-supplied product, and optional version, and pass it to
  // the Context to include in the UserAgent.
  if (params.has_user_agent_product()) {
    if (!net::HttpUtil::IsToken(params.user_agent_product())) {
      DLOG(ERROR) << "Invalid embedder product.";
      context_request.Close(ZX_ERR_INVALID_ARGS);
      return;
    }
    std::string product_tag(params.user_agent_product());
    if (params.has_user_agent_version()) {
      if (!net::HttpUtil::IsToken(params.user_agent_version())) {
        DLOG(ERROR) << "Invalid embedder version.";
        context_request.Close(ZX_ERR_INVALID_ARGS);
        return;
      }
      product_tag += "/" + params.user_agent_version();
    }
    launch_command.AppendSwitchNative(switches::kUserAgentProductAndVersion,
                                      std::move(product_tag));
  } else if (params.has_user_agent_version()) {
    DLOG(ERROR) << "Embedder version without product.";
    context_request.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  if (params.has_content_directories() &&
      !SetContentDirectoriesInCommandLine(
          std::move(*params.mutable_content_directories()), &launch_command,
          &launch_options)) {
    context_request.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  if (params.has_unsafely_treat_insecure_origins_as_secure()) {
    const std::vector<std::string>& insecure_origins =
        params.unsafely_treat_insecure_origins_as_secure();
    if (std::find(insecure_origins.begin(), insecure_origins.end(),
                  switches::kAllowRunningInsecureContent) !=
        insecure_origins.end()) {
      launch_command.AppendSwitch(switches::kAllowRunningInsecureContent);
    }
    // TODO(crbug.com/1023510): Pass the rest of the list to the Context
    // process.
  }

  if (launch_for_test_)
    launch_for_test_.Run(launch_command, launch_options);
  else
    base::LaunchProcess(launch_command, launch_options);

  // |context_request| and any DevTools channels were transferred (not copied)
  // to the Context process.
  ignore_result(context_request.TakeChannel().release());
  for (auto& channel : devtools_listener_channels)
    ignore_result(channel.release());
}

void ContextProviderImpl::SetLaunchCallbackForTest(
    LaunchCallbackForTest launch) {
  launch_for_test_ = std::move(launch);
}

void ContextProviderImpl::EnableDevTools(
    fidl::InterfaceHandle<fuchsia::web::DevToolsListener> listener,
    EnableDevToolsCallback callback) {
  devtools_listeners_.AddInterfacePtr(listener.Bind());
  callback();
}
