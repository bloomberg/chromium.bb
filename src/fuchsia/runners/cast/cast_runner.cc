// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fit/function.h>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/cast/common/constants.h"
#include "fuchsia/base/agent_manager.h"
#include "fuchsia/base/config_reader.h"
#include "fuchsia/runners/cast/cast_streaming.h"
#include "fuchsia/runners/cast/pending_cast_component.h"
#include "fuchsia/runners/common/web_content_runner.h"
#include "url/gurl.h"

namespace {

// List of services provided to the WebEngine context.
// All services must be listed in cast_runner.cmx.
static constexpr const char* kServices[] = {
    "fuchsia.accessibility.semantics.SemanticsManager",
    "fuchsia.device.NameProvider",
    "fuchsia.fonts.Provider",
    "fuchsia.input.virtualkeyboard.ControllerCreator",
    "fuchsia.intl.PropertyProvider",
    "fuchsia.logger.LogSink",
    "fuchsia.media.AudioDeviceEnumerator",
    "fuchsia.media.ProfileProvider",
    "fuchsia.media.SessionAudioConsumerFactory",
    "fuchsia.media.drm.PlayReady",
    "fuchsia.media.drm.Widevine",
    "fuchsia.mediacodec.CodecFactory",
    "fuchsia.memorypressure.Provider",
    "fuchsia.net.name.Lookup",
    "fuchsia.net.interfaces.State",
    "fuchsia.posix.socket.Provider",
    "fuchsia.process.Launcher",
    "fuchsia.settings.Display",
    "fuchsia.sysmem.Allocator",
    "fuchsia.ui.input3.Keyboard",
    "fuchsia.ui.scenic.Scenic",
    "fuchsia.vulkan.loader.Loader",

    // These services are redirected to the Agent:
    // * fuchsia.camera3.DeviceWatcher
    // * fuchsia.legacymetrics.MetricsRecorder
    // * fuchsia.media.Audio
};

// Names used to partition the Runner's persistent storage for different uses.
constexpr char kCdmDataSubdirectoryName[] = "cdm_data";
constexpr char kProfileSubdirectoryName[] = "web_profile";

// Name of the file used to detect cache erasure.
// TODO(crbug.com/1188780): Remove once an explicit cache flush signal exists.
constexpr char kSentinelFileName[] = ".sentinel";

// Ephemeral remote debugging port used by child contexts.
const uint16_t kEphemeralRemoteDebuggingPort = 0;

// Application URL for the pseudo-component providing fuchsia.web.FrameHost.
constexpr char kFrameHostComponentName[] = "cast:fuchsia.web.FrameHost";

// Application URL for the pseudo-component providing chromium.cast.DataReset.
constexpr char kDataResetComponentName[] = "cast:chromium.cast.DataReset";

// Subdirectory used to stage persistent directories to be deleted upon next
// startup.
const char kStagedForDeletionSubdirectory[] = "staged_for_deletion";

base::FilePath GetStagedForDeletionDirectoryPath() {
  base::FilePath cache_directory(base::kPersistedCacheDirectoryPath);
  return cache_directory.Append(kStagedForDeletionSubdirectory);
}

// Deletes files/directories staged for deletion during the previous run.
// We delete synchronously on main thread for simplicity. Note that this
// overall mechanism is a temporary solution. TODO(crbug.com/1146480): migrate
// to the framework mechanism of clearing session data when available.
void DeleteStagedForDeletionDirectoryIfExists() {
  const base::FilePath staged_for_deletion_directory =
      GetStagedForDeletionDirectoryPath();

  if (!PathExists(staged_for_deletion_directory))
    return;

  const base::TimeTicks started_at = base::TimeTicks::Now();
  bool result = base::DeletePathRecursively(staged_for_deletion_directory);
  if (!result) {
    LOG(ERROR) << "Failed to delete the staging directory";
    return;
  }

  LOG(WARNING) << "Deleting old persistent data took "
               << (base::TimeTicks::Now() - started_at).InMillisecondsF()
               << " ms";
}

// TODO(crbug.com/1134719): Consider removing this flag once Media Capabilities
// is supported.
void EnsureSoftwareVideoDecodersAreDisabled(
    ::fuchsia::web::ContextFeatureFlags* features) {
  if ((*features & fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER) ==
      fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER) {
    *features |= fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY;
  }
}

// Populates |params| with web data settings. Web data persistence is only
// enabled if a soft quota is explicitly specified via config-data.
void SetDataParamsForMainContext(fuchsia::web::CreateContextParams* params) {
  // Set the web data quota based on the CastRunner configuration.
  const absl::optional<base::Value>& config = cr_fuchsia::LoadPackageConfig();
  if (!config)
    return;

  constexpr char kDataQuotaBytesSwitch[] = "data-quota-bytes";
  const absl::optional<int> data_quota_bytes =
      config->FindIntPath(kDataQuotaBytesSwitch);
  if (!data_quota_bytes)
    return;

  // Allow best-effort persistent of Cast application data.
  // TODO(crbug.com/1148334): Remove the need for an explicit quota to be
  // configured, once the platform provides storage quotas.
  const auto profile_path = base::FilePath(base::kPersistedCacheDirectoryPath)
                                .Append(kProfileSubdirectoryName);
  CHECK(base::CreateDirectory(profile_path));
  params->set_data_directory(base::OpenDirectoryHandle(profile_path));
  CHECK(params->data_directory());
  params->set_data_quota_bytes(*data_quota_bytes);
}

// Populates |params| with settings to enable Widevine & PlayReady CDMs.
// CDM data persistence is always enabled, with an optional soft quota.
void SetCdmParamsForMainContext(fuchsia::web::CreateContextParams* params) {
  const absl::optional<base::Value>& config = cr_fuchsia::LoadPackageConfig();
  if (config) {
    constexpr char kCdmDataQuotaBytesSwitch[] = "cdm-data-quota-bytes";
    const absl::optional<int> cdm_data_quota_bytes =
        config->FindIntPath(kCdmDataQuotaBytesSwitch);
    if (cdm_data_quota_bytes)
      params->set_cdm_data_quota_bytes(*cdm_data_quota_bytes);
  }

  // TODO(b/154204041): Consider using isolated-persistent-storage for CDM data.
  // Create an isolated-cache-storage sub-directory for CDM data.
  const auto cdm_data_path = base::FilePath(base::kPersistedCacheDirectoryPath)
                                 .Append(kCdmDataSubdirectoryName);
  CHECK(base::CreateDirectory(cdm_data_path));
  params->set_cdm_data_directory(base::OpenDirectoryHandle(cdm_data_path));
  CHECK(params->cdm_data_directory());

  // Enable the Widevine and Playready CDMs.
  *params->mutable_features() |=
      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM;
  const char kCastPlayreadyKeySystem[] = "com.chromecast.playready";
  params->set_playready_key_system(kCastPlayreadyKeySystem);
}

// TODO(crbug.com/1120914): Remove this once Component Framework v2 can be
// used to route fuchsia.web.FrameHost capabilities cleanly.
class FrameHostComponent final : public fuchsia::sys::ComponentController {
 public:
  // Creates a FrameHostComponent with lifetime managed by |controller_request|.
  // Returns the incoming service directory, in case the CastRunner needs to use
  // it to connect to the MetricsRecorder.
  static base::WeakPtr<const sys::ServiceDirectory>
  StartAndReturnIncomingServiceDirectory(
      std::unique_ptr<base::StartupContext> startup_context,
      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
          controller_request,
      fidl::InterfaceRequestHandler<fuchsia::web::FrameHost>
          frame_host_request_handler) {
    // |frame_host_component| deletes itself when the client disconnects.
    auto* frame_host_component = new FrameHostComponent(
        std::move(startup_context), std::move(controller_request),
        std::move(frame_host_request_handler));
    return frame_host_component->weak_incoming_services_.GetWeakPtr();
  }

 private:
  FrameHostComponent(std::unique_ptr<base::StartupContext> startup_context,
                     fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                         controller_request,
                     fidl::InterfaceRequestHandler<fuchsia::web::FrameHost>
                         frame_host_request_handler)
      : startup_context_(std::move(startup_context)),
        frame_host_binding_(startup_context_->outgoing(),
                            std::move(frame_host_request_handler)),
        weak_incoming_services_(startup_context_->svc()) {
    startup_context_->ServeOutgoingDirectory();
    binding_.Bind(std::move(controller_request));
    binding_.set_error_handler([this](zx_status_t) { Kill(); });
  }
  ~FrameHostComponent() override = default;

  // fuchsia::sys::ComponentController interface.
  void Kill() override { delete this; }
  void Detach() override {
    binding_.Close(ZX_ERR_NOT_SUPPORTED);
    delete this;
  }

  const std::unique_ptr<base::StartupContext> startup_context_;
  const base::ScopedServicePublisher<fuchsia::web::FrameHost>
      frame_host_binding_;
  fidl::Binding<fuchsia::sys::ComponentController> binding_{this};

  base::WeakPtrFactory<const sys::ServiceDirectory> weak_incoming_services_;
};

// TODO(crbug.com/1120914): Remove this once Component Framework v2 can be
// used to route chromium.cast.DataReset capabilities cleanly.
class DataResetComponent final : public fuchsia::sys::ComponentController,
                                 public chromium::cast::DataReset {
 public:
  // Creates a DataResetComponent with lifetime managed by |controller_request|.
  static void Start(base::OnceCallback<bool()> delete_persistent_data,
                    std::unique_ptr<base::StartupContext> startup_context,
                    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                        controller_request) {
    new DataResetComponent(std::move(delete_persistent_data),
                           std::move(startup_context),
                           std::move(controller_request));
  }

 private:
  DataResetComponent(base::OnceCallback<bool()> delete_persistent_data,
                     std::unique_ptr<base::StartupContext> startup_context,
                     fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                         controller_request)
      : delete_persistent_data_(std::move(delete_persistent_data)),
        startup_context_(std::move(startup_context)),
        data_reset_handler_binding_(startup_context_->outgoing(), this) {
    DCHECK(delete_persistent_data_);
    startup_context_->ServeOutgoingDirectory();
    binding_.Bind(std::move(controller_request));
    binding_.set_error_handler([this](zx_status_t) { Kill(); });
  }
  ~DataResetComponent() override = default;

  // fuchsia::sys::ComponentController interface.
  void Kill() override { delete this; }
  void Detach() override {
    binding_.Close(ZX_ERR_NOT_SUPPORTED);
    delete this;
  }

  // chromium::cast::DataReset interface.
  void DeletePersistentData(DeletePersistentDataCallback callback) override {
    if (!delete_persistent_data_) {
      // Repeated requests to DeletePersistentData are not supported.
      binding_.Close(ZX_ERR_NOT_SUPPORTED);
      return;
    }
    callback(std::move(delete_persistent_data_).Run());
  }

  base::OnceCallback<bool()> delete_persistent_data_;
  std::unique_ptr<base::StartupContext> startup_context_;
  const base::ScopedServiceBinding<chromium::cast::DataReset>
      data_reset_handler_binding_;
  fidl::Binding<fuchsia::sys::ComponentController> binding_{this};
};

}  // namespace

CastRunner::CastRunner(cr_fuchsia::WebInstanceHost* web_instance_host,
                       bool is_headless)
    : web_instance_host_(web_instance_host),
      is_headless_(is_headless),
      main_services_(std::make_unique<base::FilteredServiceDirectory>(
          base::ComponentContextForProcess()->svc().get())),
      main_context_(std::make_unique<WebContentRunner>(
          web_instance_host_,
          base::BindRepeating(&CastRunner::GetMainContextParams,
                              base::Unretained(this)))),
      isolated_services_(std::make_unique<base::FilteredServiceDirectory>(
          base::ComponentContextForProcess()->svc().get())) {
  // Delete persisted data staged for deletion during the previous run.
  DeleteStagedForDeletionDirectoryIfExists();

  // Specify the services to connect via the Runner process' service directory.
  for (const char* name : kServices) {
    zx_status_t status = main_services_->AddService(name);
    ZX_CHECK(status == ZX_OK, status)
        << "AddService(" << name << ") to main failed";
    status = isolated_services_->AddService(name);
    ZX_CHECK(status == ZX_OK, status)
        << "AddService(" << name << ") to isolated failed";
  }

  // Add handlers to main context's service directory for redirected services.
  zx_status_t status =
      main_services_->outgoing_directory()
          ->AddPublicService<fuchsia::media::Audio>(
              fit::bind_member(this, &CastRunner::OnAudioServiceRequest));
  ZX_CHECK(status == ZX_OK, status) << "AddPublicService(Audio) to main failed";
  status = main_services_->outgoing_directory()
               ->AddPublicService<fuchsia::camera3::DeviceWatcher>(
                   fit::bind_member(this, &CastRunner::OnCameraServiceRequest));
  ZX_CHECK(status == ZX_OK, status)
      << "AddPublicService(DeviceWatcher) to main failed";
  status = main_services_->outgoing_directory()
               ->AddPublicService<fuchsia::legacymetrics::MetricsRecorder>(
                   fit::bind_member(
                       this, &CastRunner::OnMetricsRecorderServiceRequest));
  ZX_CHECK(status == ZX_OK, status)
      << "AddPublicService(MetricsRecorder) to main failed";

  // Isolated contexts can use the normal Audio service, and don't record
  // metrics.
  status = isolated_services_->AddService(fuchsia::media::Audio::Name_);
  ZX_CHECK(status == ZX_OK, status) << "AddService(Audio) to isolated failed";
}

CastRunner::~CastRunner() = default;

void CastRunner::StartComponent(
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  // Verify that |package| specifies a Cast URI, and pull the app-Id from it.
  constexpr char kCastPresentationUrlScheme[] = "cast";
  constexpr char kCastSecurePresentationUrlScheme[] = "casts";

  GURL cast_url(package.resolved_url);
  if (!cast_url.is_valid() ||
      (!cast_url.SchemeIs(kCastPresentationUrlScheme) &&
       !cast_url.SchemeIs(kCastSecurePresentationUrlScheme)) ||
      cast_url.GetContent().empty()) {
    LOG(ERROR) << "Rejected invalid URL: " << cast_url;
    return;
  }

  auto startup_context =
      std::make_unique<base::StartupContext>(std::move(startup_info));

  // If the persistent cache directory was erased then re-create the main Cast
  // app Context.
  if (WasPersistedCacheErased()) {
    LOG(WARNING) << "Cache erased. Restarting web.Context.";
    // The sentinel file will be re-created the next time CreateContextParams
    // are request for the main web.Context.
    was_cache_sentinel_created_ = false;
    main_context_->DestroyWebContext();
  }

  if (cors_exempt_headers_) {
    StartComponentInternal(cast_url, std::move(startup_context),
                           std::move(controller_request));
    return;
  }

  // Start a request for the CORS-exempt headers list via the component's
  // incoming service-directory, unless a request is already in-progress.
  // This assumes that the set of CORS-exempt headers is the same for all
  // components hosted by this Runner.
  if (!cors_exempt_headers_provider_) {
    startup_context->svc()->Connect(cors_exempt_headers_provider_.NewRequest());

    cors_exempt_headers_provider_.set_error_handler([this](zx_status_t status) {
      ZX_LOG(ERROR, status) << "CorsExemptHeaderProvider disconnected.";
      // Clearing queued callbacks closes resources associated with those
      // component launch requests, effectively causing them to fail.
      on_have_cors_exempt_headers_.clear();
    });

    cors_exempt_headers_provider_->GetCorsExemptHeaderNames(
        [this](std::vector<std::vector<uint8_t>> header_names) {
          cors_exempt_headers_provider_.Unbind();
          cors_exempt_headers_ = std::move(header_names);
          for (auto& callback : on_have_cors_exempt_headers_)
            std::move(callback).Run();
          on_have_cors_exempt_headers_.clear();
        });
  }

  // Queue the component launch to be resumed once the header list is available.
  on_have_cors_exempt_headers_.push_back(base::BindOnce(
      &CastRunner::StartComponentInternal, base::Unretained(this), cast_url,
      std::move(startup_context), std::move(controller_request)));
}

bool CastRunner::DeletePersistentData() {
  // Set data reset flag so that new components are not being started.
  data_reset_in_progress_ = true;

  // Create the staging directory.
  base::FilePath staged_for_deletion_directory =
      GetStagedForDeletionDirectoryPath();
  base::File::Error file_error;
  bool result = base::CreateDirectoryAndGetError(staged_for_deletion_directory,
                                                 &file_error);
  if (!result) {
    LOG(ERROR) << "Failed to create the staging directory, error: "
               << file_error;
    return false;
  }

  // Stage everything under `/cache` for deletion.
  const base::FilePath cache_directory(base::kPersistedCacheDirectoryPath);
  base::FileEnumerator enumerator(
      cache_directory, /*recursive=*/false,
      base::FileEnumerator::FileType::FILES |
          base::FileEnumerator::FileType::DIRECTORIES);
  for (base::FilePath current = enumerator.Next(); !current.empty();
       current = enumerator.Next()) {
    // Skip the staging directory itself.
    if (current == staged_for_deletion_directory) {
      continue;
    }

    base::FilePath destination =
        staged_for_deletion_directory.Append(current.BaseName());
    result = base::Move(current, destination);
    if (!result) {
      LOG(ERROR) << "Failed to move " << current << " to " << destination;
      return false;
    }
  }

  return true;
}

void CastRunner::LaunchPendingComponent(PendingCastComponent* pending_component,
                                        CastComponent::Params params) {
  DCHECK(cors_exempt_headers_);

  // TODO(crbug.com/1082821): Remove |web_content_url| once the Cast Streaming
  // Receiver component has been implemented.
  GURL web_content_url(params.application_config.web_url());
  if (IsAppConfigForCastStreaming(params.application_config))
    web_content_url = GURL(kCastStreamingWebUrl);

  absl::optional<fuchsia::web::CreateContextParams> create_context_params =
      GetContextParamsForAppConfig(&params.application_config);

  WebContentRunner* component_owner = main_context_.get();
  if (create_context_params) {
    component_owner = CreateIsolatedContextForParams(
        std::move(create_context_params.value()));
  }

  auto cast_component = std::make_unique<CastComponent>(
      base::StrCat({"cast:", pending_component->app_id()}), component_owner,
      std::move(params), is_headless_);

  // Start the component, which creates and configures the web.Frame, and load
  // the specified web content into it.
  cast_component->SetOnDestroyedCallback(
      base::BindOnce(&CastRunner::OnComponentDestroyed, base::Unretained(this),
                     base::Unretained(cast_component.get())));
  cast_component->StartComponent();
  cast_component->LoadUrl(std::move(web_content_url),
                          std::vector<fuchsia::net::http::Header>());

  if (component_owner == main_context_.get()) {
    // For components in the main Context the cache sentinel file should have
    // been created as a side-effect of |CastComponent::StartComponent()|.
    DCHECK(was_cache_sentinel_created_);

    // If this component has the microphone permission then use it to route
    // Audio service requests through.
    if (cast_component->HasWebPermission(
            fuchsia::web::PermissionType::MICROPHONE)) {
      if (first_audio_capturer_agent_url_.empty()) {
        first_audio_capturer_agent_url_ = cast_component->agent_url();
      } else {
        LOG_IF(WARNING,
               first_audio_capturer_agent_url_ != cast_component->agent_url())
            << "Audio capturer already in use for different agent. "
               "Current agent: "
            << cast_component->agent_url();
      }
      audio_capturer_components_.emplace(cast_component.get());
    }

    if (cast_component->HasWebPermission(
            fuchsia::web::PermissionType::CAMERA)) {
      if (first_video_capturer_agent_url_.empty()) {
        first_video_capturer_agent_url_ = cast_component->agent_url();
      } else {
        LOG_IF(WARNING,
               first_video_capturer_agent_url_ != cast_component->agent_url())
            << "Video capturer already in use for different agent. "
               "Current agent: "
            << cast_component->agent_url();
      }
      video_capturer_components_.emplace(cast_component.get());
    }
  }

  // Do not launch new main context components while data reset is in progress,
  // so that they don't create new persisted state. We expect the session
  // to be restarted shortly after data reset completes.
  if (data_reset_in_progress_ && component_owner == main_context_.get()) {
    pending_components_.erase(pending_component);
    return;
  }

  // Register the new component and clean up the |pending_component|.
  component_owner->RegisterComponent(std::move(cast_component));
  pending_components_.erase(pending_component);
}

void CastRunner::CancelPendingComponent(
    PendingCastComponent* pending_component) {
  size_t count = pending_components_.erase(pending_component);
  DCHECK_EQ(count, 1u);
}

void CastRunner::OnComponentDestroyed(CastComponent* component) {
  audio_capturer_components_.erase(component);
  video_capturer_components_.erase(component);
}

fuchsia::web::CreateContextParams CastRunner::GetCommonContextParams() {
  fuchsia::web::CreateContextParams params;
  params.set_features(fuchsia::web::ContextFeatureFlags::AUDIO);

  if (is_headless_) {
    LOG(WARNING) << "Running in headless mode.";
    *params.mutable_features() |= fuchsia::web::ContextFeatureFlags::HEADLESS;
  } else {
    *params.mutable_features() |=
        fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
        fuchsia::web::ContextFeatureFlags::VULKAN;
  }

  // When tests require that VULKAN be disabled, DRM must also be disabled.
  if (disable_vulkan_for_test_) {
    *params.mutable_features() &=
        ~(fuchsia::web::ContextFeatureFlags::VULKAN |
          fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER);
  }

  // If there is a list of headers to exempt from CORS checks, pass the list
  // along to the Context.
  CHECK(cors_exempt_headers_);
  if (!cors_exempt_headers_->empty())
    params.set_cors_exempt_headers(*cors_exempt_headers_);

  return params;
}

fuchsia::web::CreateContextParams CastRunner::GetMainContextParams() {
  fuchsia::web::CreateContextParams params = GetCommonContextParams();
  *params.mutable_features() |=
      fuchsia::web::ContextFeatureFlags::NETWORK |
      fuchsia::web::ContextFeatureFlags::LEGACYMETRICS |
      fuchsia::web::ContextFeatureFlags::KEYBOARD |
      fuchsia::web::ContextFeatureFlags::VIRTUAL_KEYBOARD;
  EnsureSoftwareVideoDecodersAreDisabled(params.mutable_features());
  params.set_remote_debugging_port(CastRunner::kRemoteDebuggingPort);

  params.set_user_agent_product("CrKey");
  params.set_user_agent_version(chromecast::kFrozenCrKeyValue);

  zx_status_t status = main_services_->ConnectClient(
      params.mutable_service_directory()->NewRequest());
  ZX_CHECK(status == ZX_OK, status) << "ConnectClient failed";

  if (!disable_vulkan_for_test_)
    SetCdmParamsForMainContext(&params);

  SetDataParamsForMainContext(&params);

  // Create a sentinel file to detect if the cache is erased.
  // TODO(crbug.com/1188780): Remove once an explicit cache flush signal exists.
  CreatePersistedCacheSentinel();

  // TODO(crbug.com/1023514): Remove this switch when it is no longer
  // necessary.
  params.set_unsafely_treat_insecure_origins_as_secure(
      {"allow-running-insecure-content", "disable-mixed-content-autoupgrade"});

  return params;
}

fuchsia::web::CreateContextParams
CastRunner::GetIsolatedContextParamsWithFuchsiaDirs(
    std::vector<fuchsia::web::ContentDirectoryProvider> content_directories) {
  fuchsia::web::CreateContextParams params = GetCommonContextParams();
  EnsureSoftwareVideoDecodersAreDisabled(params.mutable_features());
  *params.mutable_features() |= fuchsia::web::ContextFeatureFlags::NETWORK;
  params.set_remote_debugging_port(kEphemeralRemoteDebuggingPort);
  params.set_content_directories(std::move(content_directories));
  zx_status_t status = isolated_services_->ConnectClient(
      params.mutable_service_directory()->NewRequest());
  ZX_CHECK(status == ZX_OK, status) << "ConnectClient failed";
  return params;
}

fuchsia::web::CreateContextParams
CastRunner::GetIsolatedContextParamsForCastStreaming() {
  fuchsia::web::CreateContextParams params = GetCommonContextParams();
  ApplyCastStreamingContextParams(&params);
  params.set_remote_debugging_port(kEphemeralRemoteDebuggingPort);
  // TODO(crbug.com/1069746): Use a different FilteredServiceDirectory for Cast
  // Streaming Contexts.
  zx_status_t status = main_services_->ConnectClient(
      params.mutable_service_directory()->NewRequest());
  ZX_CHECK(status == ZX_OK, status) << "ConnectClient failed";
  return params;
}

absl::optional<fuchsia::web::CreateContextParams>
CastRunner::GetContextParamsForAppConfig(
    chromium::cast::ApplicationConfig* app_config) {
  if (IsAppConfigForCastStreaming(*app_config)) {
    // TODO(crbug.com/1082821): Remove this once the CastStreamingReceiver
    // Component has been implemented.
    return absl::make_optional(GetIsolatedContextParamsForCastStreaming());
  }

  const bool is_isolated_app =
      app_config->has_content_directories_for_isolated_application();
  if (is_isolated_app) {
    return absl::make_optional(
        GetIsolatedContextParamsWithFuchsiaDirs(std::move(
            *app_config
                 ->mutable_content_directories_for_isolated_application())));
  }

  // No need to create an isolated context in other cases.
  return absl::nullopt;
}

WebContentRunner* CastRunner::CreateIsolatedContextForParams(
    fuchsia::web::CreateContextParams create_context_params) {
  // Create an isolated context which will own the CastComponent.
  auto context = std::make_unique<WebContentRunner>(
      web_instance_host_, std::move(create_context_params));
  context->SetOnEmptyCallback(
      base::BindOnce(&CastRunner::OnIsolatedContextEmpty,
                     base::Unretained(this), base::Unretained(context.get())));
  WebContentRunner* raw_context = context.get();
  isolated_contexts_.insert(std::move(context));
  return raw_context;
}

void CastRunner::OnIsolatedContextEmpty(WebContentRunner* context) {
  auto it = isolated_contexts_.find(context);
  DCHECK(it != isolated_contexts_.end());
  isolated_contexts_.erase(it);
}

void CastRunner::OnAudioServiceRequest(
    fidl::InterfaceRequest<fuchsia::media::Audio> request) {
  // If we have a component that allows AudioCapturer access then redirect the
  // fuchsia.media.Audio requests to the corresponding agent.
  if (!audio_capturer_components_.empty()) {
    CastComponent* capturer_component = *audio_capturer_components_.begin();
    capturer_component->ConnectAudio(std::move(request));
    return;
  }

  // Otherwise use the Runner's fuchsia.media.Audio service. fuchsia.media.Audio
  // may be used by frames without MICROPHONE permission to create AudioRenderer
  // instance.
  base::ComponentContextForProcess()->svc()->Connect(std::move(request));
}

void CastRunner::OnCameraServiceRequest(
    fidl::InterfaceRequest<fuchsia::camera3::DeviceWatcher> request) {
  // If we have a component that allows camera access then redirect the
  // fuchsia.camera3.DeviceWatcher requests to the corresponding agent.
  if (!video_capturer_components_.empty()) {
    CastComponent* capturer_component = *video_capturer_components_.begin();
    capturer_component->ConnectDeviceWatcher(std::move(request));
    return;
  }

  // fuchsia.camera3.DeviceWatcher may be requested while none of the running
  // apps have the CAMERA permission. Return ZX_ERR_UNAVAILABLE, which implies
  // that the client should try connecting again later, since the service may
  // become available after a web.Frame with camera access is created.
  request.Close(ZX_ERR_UNAVAILABLE);
}

void CastRunner::OnMetricsRecorderServiceRequest(
    fidl::InterfaceRequest<fuchsia::legacymetrics::MetricsRecorder> request) {
  // TODO(crbug.com/1065707): Remove this hack once the service can be routed
  // through the Runner's incoming service directory, in Component Framework v2.

  // Attempt to connect via any CastComponent's incoming services.
  WebComponent* any_component = main_context_->GetAnyComponent();
  if (any_component) {
    VLOG(1) << "Connecting MetricsRecorder via CastComponent.";
    CastComponent* component = reinterpret_cast<CastComponent*>(any_component);
    component->ConnectMetricsRecorder(std::move(request));
    return;
  }

  // Attempt to connect via a FrameHostComponent's services, if available.
  if (frame_host_component_incoming_services_) {
    VLOG(1) << "Connecting MetricsRecorder via FrameHostComponent.";
    frame_host_component_incoming_services_->Connect(std::move(request));
    return;
  }

  LOG(WARNING) << "Ignoring MetricsRecorder request.";
}

void CastRunner::StartComponentInternal(
    const GURL& url,
    std::unique_ptr<base::StartupContext> startup_context,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  // TODO(crbug.com/1120914): Remove this once Component Framework v2 can be
  // used to route fuchsia.web.FrameHost capabilities cleanly.
  if (enable_frame_host_component_ && (url.spec() == kFrameHostComponentName)) {
    frame_host_component_incoming_services_ =
        FrameHostComponent::StartAndReturnIncomingServiceDirectory(
            std::move(startup_context), std::move(controller_request),
            main_context_->GetFrameHostRequestHandler());
    return;
  }

  // TODO(crbug.com/1120914): Remove this once Component Framework v2 can be
  // used to route chromium.cast.DataReset capabilities cleanly.
  if (url.spec() == kDataResetComponentName) {
    DataResetComponent::Start(base::BindOnce(&CastRunner::DeletePersistentData,
                                             base::Unretained(this)),
                              std::move(startup_context),
                              std::move(controller_request));
    return;
  }

  pending_components_.emplace(std::make_unique<PendingCastComponent>(
      this, std::move(startup_context), std::move(controller_request),
      url.GetContent()));
}

static base::FilePath SentinelFilePath() {
  return base::FilePath(base::kPersistedCacheDirectoryPath)
      .Append(kSentinelFileName);
}

void CastRunner::CreatePersistedCacheSentinel() {
  base::WriteFile(SentinelFilePath(), "");
  was_cache_sentinel_created_ = true;
}

bool CastRunner::WasPersistedCacheErased() {
  if (!was_cache_sentinel_created_)
    return false;
  return !base::PathExists(SentinelFilePath());
}
