// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/cast_component.h"

#include <lib/fidl/cpp/binding.h>
#include <lib/ui/scenic/cpp/view_ref_pair.h>
#include <algorithm>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/path_service.h"
#include "base/task/current_thread.h"
#include "components/cast/message_port/message_port_fuchsia.h"
#include "fuchsia/base/agent_manager.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/cast_runner.h"
#include "fuchsia/runners/cast/cast_streaming.h"
#include "fuchsia/runners/cast/create_web_message.h"
#include "fuchsia/runners/common/web_component.h"

namespace {

constexpr int kBindingsFailureExitCode = 129;
constexpr int kRewriteRulesProviderDisconnectExitCode = 130;

fuchsia::web::ConsoleLogLevel SeverityToConsoleLogLevel(
    fuchsia::diagnostics::Severity severity) {
  switch (severity) {
    case fuchsia::diagnostics::Severity::TRACE:
    case fuchsia::diagnostics::Severity::DEBUG:
      return fuchsia::web::ConsoleLogLevel::DEBUG;
    case fuchsia::diagnostics::Severity::INFO:
      return fuchsia::web::ConsoleLogLevel::INFO;
    case fuchsia::diagnostics::Severity::WARN:
      return fuchsia::web::ConsoleLogLevel::WARN;
    case fuchsia::diagnostics::Severity::ERROR:
      return fuchsia::web::ConsoleLogLevel::ERROR;
    case fuchsia::diagnostics::Severity::FATAL:
      // FATAL means none per the FIDL definition.
      return fuchsia::web::ConsoleLogLevel::NONE;
  }

  // The safest thing to do for unrecognized values is to not log.
  return fuchsia::web::ConsoleLogLevel::NONE;
}

}  // namespace

CastComponent::Params::Params() = default;
CastComponent::Params::Params(Params&&) = default;
CastComponent::Params::~Params() = default;

bool CastComponent::Params::AreComplete() const {
  if (application_config.IsEmpty())
    return false;
  if (!api_bindings_client->HasBindings())
    return false;
  if (!initial_url_rewrite_rules)
    return false;
  if (!media_session_id)
    return false;
  return true;
}

CastComponent::CastComponent(base::StringPiece debug_name,
                             WebContentRunner* runner,
                             CastComponent::Params params,
                             bool is_headless)
    : WebComponent(debug_name,
                   runner,
                   std::move(params.startup_context),
                   std::move(params.controller_request)),
      is_headless_(is_headless),
      agent_manager_(std::move(params.agent_manager)),
      application_config_(std::move(params.application_config)),
      url_rewrite_rules_provider_(std::move(params.url_rewrite_rules_provider)),
      initial_url_rewrite_rules_(
          std::move(params.initial_url_rewrite_rules.value())),
      api_bindings_client_(std::move(params.api_bindings_client)),
      application_context_(params.application_context.Bind()),
      media_session_id_(params.media_session_id.value()),
      headless_disconnect_watch_(FROM_HERE) {
  base::AutoReset<bool> constructor_active_reset(&constructor_active_, true);
}

CastComponent::~CastComponent() = default;

void CastComponent::SetOnDestroyedCallback(base::OnceClosure on_destroyed) {
  on_destroyed_ = std::move(on_destroyed);
}

void CastComponent::ConnectMetricsRecorder(
    fidl::InterfaceRequest<fuchsia::legacymetrics::MetricsRecorder> request) {
  startup_context()->svc()->Connect(std::move(request));
}

void CastComponent::ConnectAudio(
    fidl::InterfaceRequest<fuchsia::media::Audio> request) {
  agent_manager_->ConnectToAgentService(application_config_.agent_url(),
                                        std::move(request));
}

void CastComponent::ConnectDeviceWatcher(
    fidl::InterfaceRequest<fuchsia::camera3::DeviceWatcher> request) {
  agent_manager_->ConnectToAgentService(application_config_.agent_url(),
                                        std::move(request));
}

bool CastComponent::HasWebPermission(
    fuchsia::web::PermissionType permission_type) const {
  if (!application_config_.has_permissions()) {
    return false;
  }

  for (auto& permission : application_config_.permissions()) {
    if (permission.has_type() && permission.type() == permission_type) {
      return true;
    }
  }

  return false;
}

void CastComponent::StartComponent() {
  if (application_config_.has_enable_remote_debugging() &&
      application_config_.enable_remote_debugging()) {
    WebComponent::EnableRemoteDebugging();
  }

  WebComponent::StartComponent();

  connector_ = std::make_unique<NamedMessagePortConnectorFuchsia>(frame());

  url_rewrite_rules_provider_.set_error_handler([this](zx_status_t status) {
    ZX_LOG_IF(ERROR, status != ZX_OK, status)
        << "UrlRequestRewriteRulesProvider disconnected.";
    DestroyComponent(kRewriteRulesProviderDisconnectExitCode,
                     fuchsia::sys::TerminationReason::INTERNAL_ERROR);
  });
  OnRewriteRulesReceived(std::move(initial_url_rewrite_rules_));

  frame()->SetMediaSessionId(media_session_id_);
  frame()->ConfigureInputTypes(fuchsia::web::InputTypes::ALL,
                               fuchsia::web::AllowInputState::DENY);
  if (application_config_.has_initial_min_console_log_severity()) {
    frame()->SetJavaScriptLogLevel(SeverityToConsoleLogLevel(
        application_config_.initial_min_console_log_severity()));
  }

  if (IsAppConfigForCastStreaming(application_config_)) {
    // TODO(crbug.com/1082821): Remove this once the Cast Streaming Receiver
    // component has been implemented.

    // Register the MessagePort for the Cast Streaming Receiver.
    std::unique_ptr<cast_api_bindings::MessagePort> message_port_for_web_engine;
    std::unique_ptr<cast_api_bindings::MessagePort> message_port_for_agent;
    cast_api_bindings::MessagePort::CreatePair(&message_port_for_agent,
                                               &message_port_for_web_engine);
    frame()->PostMessage(
        kCastStreamingMessagePortOrigin,
        CreateWebMessage("", std::move(message_port_for_web_engine)),
        [this](fuchsia::web::Frame_PostMessage_Result result) {
          if (result.is_err()) {
            DestroyComponent(kBindingsFailureExitCode,
                             fuchsia::sys::TerminationReason::INTERNAL_ERROR);
          }
        });
    api_bindings_client_->OnPortConnected(kCastStreamingMessagePortName,
                                          std::move(message_port_for_agent));
  }

  api_bindings_client_->AttachToFrame(
      frame(), connector_.get(),
      base::BindOnce(&CastComponent::DestroyComponent, base::Unretained(this),
                     kBindingsFailureExitCode,
                     fuchsia::sys::TerminationReason::INTERNAL_ERROR));

  // Get the theme from the system service.
  frame()->SetPreferredTheme(fuchsia::settings::ThemeType::AUTO);

  // Media loading has to be unblocked by the agent via the
  // ApplicationController.
  frame()->SetBlockMediaLoading(true);

  if (application_config_.has_force_content_dimensions()) {
    frame()->ForceContentDimensions(std::make_unique<fuchsia::ui::gfx::vec2>(
        application_config_.force_content_dimensions()));
  }

  application_controller_ = std::make_unique<ApplicationControllerImpl>(
      frame(), application_context_.get());

  // Apply application-specific web permissions to the fuchsia.web.Frame.
  if (application_config_.has_permissions()) {
    // TODO(crbug.com/1136994): Replace this with the PermissionManager API
    // when available.
    const std::string origin =
        GURL(application_config_.web_url()).GetOrigin().spec();
    for (auto& permission : application_config_.permissions()) {
      fuchsia::web::PermissionDescriptor permission_clone;
      zx_status_t status = permission.Clone(&permission_clone);
      ZX_DCHECK(status == ZX_OK, status);
      const bool all_origins =
          permission_clone.has_type() &&
          (permission_clone.type() ==
           fuchsia::web::PermissionType::PROTECTED_MEDIA_IDENTIFIER);
      frame()->SetPermissionState(std::move(permission_clone),
                                  all_origins ? "*" : origin,
                                  fuchsia::web::PermissionState::GRANTED);
    }
  }
}

void CastComponent::DestroyComponent(int64_t exit_code,
                                     fuchsia::sys::TerminationReason reason) {
  DCHECK(!constructor_active_);

  std::move(on_destroyed_).Run();

  // If the component EXITED then pass the |exit_code| to the Agent, to allow it
  // to distinguish graceful termination from crashes.
  if (reason == fuchsia::sys::TerminationReason::EXITED &&
      application_controller_) {
    application_context_->OnApplicationExit(exit_code);
  }

  // frame() is about to be destroyed, so there is no need to perform cleanup
  // such as removing before-load JavaScripts.
  api_bindings_client_->DetachFromFrame(frame());
  connector_->DetachFromFrame();

  WebComponent::DestroyComponent(exit_code, reason);
}

void CastComponent::OnRewriteRulesReceived(
    std::vector<fuchsia::web::UrlRequestRewriteRule> rewrite_rules) {
  frame()->SetUrlRequestRewriteRules(std::move(rewrite_rules), [this]() {
    url_rewrite_rules_provider_->GetUrlRequestRewriteRules(
        fit::bind_member(this, &CastComponent::OnRewriteRulesReceived));
  });
}

void CastComponent::OnNavigationStateChanged(
    fuchsia::web::NavigationState change,
    OnNavigationStateChangedCallback callback) {
  if (change.has_is_main_document_loaded() &&
      change.is_main_document_loaded()) {
    std::string connect_message;
    std::unique_ptr<cast_api_bindings::MessagePort> connect_port;
    connector_->GetConnectMessage(&connect_message, &connect_port);

    // Send the NamedMessagePortConnector handshake to the page.
    frame()->PostMessage(
        "*", CreateWebMessage(connect_message, std::move(connect_port)),
        [](fuchsia::web::Frame_PostMessage_Result result) {
          DCHECK(result.is_response());
        });
  }

  WebComponent::OnNavigationStateChanged(std::move(change),
                                         std::move(callback));
}

void CastComponent::CreateView(
    zx::eventpair view_token,
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
    fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services) {
  scenic::ViewRefPair view_ref_pair = scenic::ViewRefPair::New();
  CreateViewWithViewRef(std::move(view_token),
                        std::move(view_ref_pair.control_ref),
                        std::move(view_ref_pair.view_ref));
}

void CastComponent::CreateViewWithViewRef(
    zx::eventpair view_token,
    fuchsia::ui::views::ViewRefControl control_ref,
    fuchsia::ui::views::ViewRef view_ref) {
  if (is_headless_) {
    // For headless CastComponents, |view_token| does not actually connect to a
    // Scenic View. It is merely used as a conduit for propagating termination
    // signals.
    headless_view_token_ = std::move(view_token);
    base::CurrentIOThread::Get()->WatchZxHandle(
        headless_view_token_.get(), false /* persistent */,
        ZX_SOCKET_PEER_CLOSED, &headless_disconnect_watch_, this);

    frame()->EnableHeadlessRendering();
    return;
  }

  WebComponent::CreateViewWithViewRef(
      std::move(view_token), std::move(control_ref), std::move(view_ref));
}

void CastComponent::OnZxHandleSignalled(zx_handle_t handle,
                                        zx_signals_t signals) {
  DCHECK_EQ(signals, ZX_SOCKET_PEER_CLOSED);
  DCHECK(is_headless_);

  frame()->DisableHeadlessRendering();
}
