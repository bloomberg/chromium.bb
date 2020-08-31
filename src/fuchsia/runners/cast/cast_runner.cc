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
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/filtered_service_directory.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "fuchsia/base/agent_manager.h"
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
    "fuchsia.intl.PropertyProvider",
    "fuchsia.logger.LogSink",
    "fuchsia.media.SessionAudioConsumerFactory",
    "fuchsia.media.drm.PlayReady",
    "fuchsia.media.drm.Widevine",
    "fuchsia.mediacodec.CodecFactory",
    "fuchsia.memorypressure.Provider",
    "fuchsia.net.NameLookup",
    "fuchsia.netstack.Netstack",
    "fuchsia.posix.socket.Provider",
    "fuchsia.process.Launcher",
    "fuchsia.sysmem.Allocator",
    "fuchsia.ui.input.ImeService",
    "fuchsia.ui.input.ImeVisibilityService",
    "fuchsia.ui.scenic.Scenic",
    "fuchsia.vulkan.loader.Loader",

    // These services are redirected to the Agent:
    // * fuchsia.camera3.DeviceWatcher
    // * fuchsia.legacymetrics.MetricsRecorder
    // * fuchsia.media.Audio
};

bool IsPermissionGrantedInAppConfig(
    const chromium::cast::ApplicationConfig& application_config,
    fuchsia::web::PermissionType permission_type) {
  if (application_config.has_permissions()) {
    for (auto& permission : application_config.permissions()) {
      if (permission.has_type() && permission.type() == permission_type)
        return true;
    }
  }
  return false;
}

}  // namespace

CastRunner::CastRunner(bool is_headless)
    : is_headless_(is_headless),
      main_services_(std::make_unique<base::fuchsia::FilteredServiceDirectory>(
          base::fuchsia::ComponentContextForCurrentProcess()->svc().get())),
      main_context_(std::make_unique<WebContentRunner>(
          base::BindRepeating(&CastRunner::GetMainContextParams,
                              base::Unretained(this)))),
      isolated_services_(
          std::make_unique<base::fuchsia::FilteredServiceDirectory>(
              base::fuchsia::ComponentContextForCurrentProcess()
                  ->svc()
                  .get())) {
  // Specify the services to connect via the Runner process' service directory.
  for (const char* name : kServices) {
    main_services_->AddService(name);
    isolated_services_->AddService(name);
  }

  // Add handlers to main context's service directory for redirected services.
  main_services_->outgoing_directory()->AddPublicService<fuchsia::media::Audio>(
      fit::bind_member(this, &CastRunner::OnAudioServiceRequest));
  main_services_->outgoing_directory()
      ->AddPublicService<fuchsia::camera3::DeviceWatcher>(
          fit::bind_member(this, &CastRunner::OnCameraServiceRequest));
  main_services_->outgoing_directory()
      ->AddPublicService<fuchsia::legacymetrics::MetricsRecorder>(
          fit::bind_member(this, &CastRunner::OnMetricsRecorderServiceRequest));

  // Isolated contexts can use the normal Audio service, and don't record
  // metrics.
  isolated_services_->AddService(fuchsia::media::Audio::Name_);
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

  pending_components_.emplace(std::make_unique<PendingCastComponent>(
      this,
      std::make_unique<base::fuchsia::StartupContext>(std::move(startup_info)),
      std::move(controller_request), cast_url.GetContent()));
}

void CastRunner::SetOnMainContextLostCallbackForTest(
    base::OnceClosure on_context_lost) {
  main_context_->SetOnContextLostCallbackForTest(std::move(on_context_lost));
}

void CastRunner::LaunchPendingComponent(PendingCastComponent* pending_component,
                                        CastComponent::Params params) {
  WebContentRunner* component_owner = main_context_.get();

  // Save the list of CORS exemptions so that they can be used in Context
  // creation parameters.
  cors_exempt_headers_ = pending_component->TakeCorsExemptHeaders();

  const bool is_isolated =
      params.application_config
          .has_content_directories_for_isolated_application();
  if (is_isolated) {
    // Create an isolated context which will own the CastComponent.
    auto context =
        std::make_unique<WebContentRunner>(GetIsolatedContextParams(std::move(
            *params.application_config
                 .mutable_content_directories_for_isolated_application())));
    context->SetOnEmptyCallback(base::BindOnce(
        &CastRunner::OnIsolatedContextEmpty, base::Unretained(this),
        base::Unretained(context.get())));
    component_owner = context.get();
    isolated_contexts_.insert(std::move(context));
  }

  // Launch the URL specified in the component |params|.
  GURL app_url = GURL(params.application_config.web_url());
  auto cast_component = std::make_unique<CastComponent>(
      component_owner, std::move(params), is_headless_);
  cast_component->SetOnDestroyedCallback(
      base::BindOnce(&CastRunner::OnComponentDestroyed, base::Unretained(this),
                     base::Unretained(cast_component.get())));
  cast_component->StartComponent();
  cast_component->LoadUrl(std::move(app_url),
                          std::vector<fuchsia::net::http::Header>());

  if (!is_isolated) {
    // If this component has the microphone permission then use it to route
    // Audio service requests through.
    if (IsPermissionGrantedInAppConfig(
            cast_component->application_config(),
            fuchsia::web::PermissionType::MICROPHONE)) {
      audio_capturer_component_ = cast_component.get();
    }

    if (IsPermissionGrantedInAppConfig(cast_component->application_config(),
                                       fuchsia::web::PermissionType::CAMERA)) {
      video_capturer_component_ = cast_component.get();
    }
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
  if (component == audio_capturer_component_)
    audio_capturer_component_ = nullptr;
}

fuchsia::web::CreateContextParams CastRunner::GetCommonContextParams() {
  fuchsia::web::CreateContextParams params;
  params.set_features(fuchsia::web::ContextFeatureFlags::AUDIO |
                      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM);

  if (is_headless_) {
    LOG(WARNING) << "Running in headless mode.";
    *params.mutable_features() |= fuchsia::web::ContextFeatureFlags::HEADLESS;
  } else {
    *params.mutable_features() |=
        fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER |
        fuchsia::web::ContextFeatureFlags::HARDWARE_VIDEO_DECODER_ONLY |
        fuchsia::web::ContextFeatureFlags::VULKAN;
  }

  const char kCastPlayreadyKeySystem[] = "com.chromecast.playready";
  params.set_playready_key_system(kCastPlayreadyKeySystem);

  // TODO(b/141956135): Use CrKey version provided by the Agent.
  params.set_user_agent_product("CrKey");
  params.set_user_agent_version("1.43.000000");

  params.set_remote_debugging_port(CastRunner::kRemoteDebuggingPort);

  // When tests require that VULKAN be disabled, DRM must also be disabled.
  if (disable_vulkan_for_test_) {
    *params.mutable_features() &=
        ~(fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM |
          fuchsia::web::ContextFeatureFlags::VULKAN);
    params.clear_playready_key_system();
  }

  // If there is a list of headers to exempt from CORS checks, pass the list
  // along to the Context.
  if (!cors_exempt_headers_.empty())
    params.set_cors_exempt_headers(cors_exempt_headers_);

  return params;
}

fuchsia::web::CreateContextParams CastRunner::GetMainContextParams() {
  fuchsia::web::CreateContextParams params = GetCommonContextParams();
  *params.mutable_features() |=
      fuchsia::web::ContextFeatureFlags::NETWORK |
      fuchsia::web::ContextFeatureFlags::LEGACYMETRICS;
  main_services_->ConnectClient(
      params.mutable_service_directory()->NewRequest());

  // TODO(crbug.com/1023514): Remove this switch when it is no longer
  // necessary.
  params.set_unsafely_treat_insecure_origins_as_secure(
      {"allow-running-insecure-content"});

  return params;
}

fuchsia::web::CreateContextParams CastRunner::GetIsolatedContextParams(
    std::vector<fuchsia::web::ContentDirectoryProvider> content_directories) {
  fuchsia::web::CreateContextParams params = GetCommonContextParams();
  params.set_content_directories(std::move(content_directories));
  isolated_services_->ConnectClient(
      params.mutable_service_directory()->NewRequest());
  return params;
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
  if (audio_capturer_component_) {
    audio_capturer_component_->agent_manager()->ConnectToAgentService(
        audio_capturer_component_->application_config().agent_url(),
        std::move(request));
    return;
  }

  // Otherwise use the Runner's fuchsia.media.Audio service. fuchsia.media.Audio
  // may be used by frames without MICRIPHONE permission to create AudioRenderer
  // instance.
  base::fuchsia::ComponentContextForCurrentProcess()->svc()->Connect(
      std::move(request));
}

void CastRunner::OnCameraServiceRequest(
    fidl::InterfaceRequest<fuchsia::camera3::DeviceWatcher> request) {
  // If we have a component that allows camera access then redirect the
  // fuchsia.camera3.DeviceWatcher requests to the corresponding agent.
  if (video_capturer_component_) {
    video_capturer_component_->agent_manager()->ConnectToAgentService(
        video_capturer_component_->application_config().agent_url(),
        std::move(request));
    return;
  }

  LOG(WARNING) << "fuchsia.camera3.DeviceWatcher request was received while no "
                  "apps with the CAMERA permission are running.";
  // Drop the request.
}

void CastRunner::OnMetricsRecorderServiceRequest(
    fidl::InterfaceRequest<fuchsia::legacymetrics::MetricsRecorder> request) {
  // TODO(https://crbug.com/1065707): Remove this hack once Runners are using
  // Component Framework v2.
  CastComponent* component =
      reinterpret_cast<CastComponent*>(main_context_->GetAnyComponent());
  DCHECK(component);

  component->agent_manager()->ConnectToAgentService(
      component->application_config().agent_url(), std::move(request));
}
