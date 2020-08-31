// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/pending_cast_component.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "fuchsia/base/agent_manager.h"

PendingCastComponent::PendingCastComponent(
    Delegate* delegate,
    std::unique_ptr<base::fuchsia::StartupContext> startup_context,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request,
    base::StringPiece app_id)
    : delegate_(delegate) {
  DCHECK(startup_context);
  DCHECK(controller_request);

  // Store the supplied CastComponent parameters in |params_|.
  params_.startup_context = std::move(startup_context);
  params_.controller_request = std::move(controller_request);

  // Request the application's configuration, including the identity of the
  // Agent that should provide component-specific resources, e.g. API bindings.
  // TODO(https://crbug.com/1065707): Access the ApplicationConfigManager via
  // the Runner's incoming service directory once it is available there.
  params_.startup_context->svc()->Connect(
      application_config_manager_.NewRequest());
  application_config_manager_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "ApplicationConfigManager disconnected.";
    delegate_->CancelPendingComponent(this);
  });
  application_config_manager_->GetConfig(
      app_id.as_string(),
      fit::bind_member(this,
                       &PendingCastComponent::OnApplicationConfigReceived));

  // Create the AgentManager through which component-specific Agent services
  // will be connected.
  // TODO(https://crbug.com/1065709): Migrate off the ConnectToAgentService()
  // API and remove the AgentManager.
  params_.agent_manager = std::make_unique<cr_fuchsia::AgentManager>(
      params_.startup_context->component_context()->svc().get());

  RequestCorsExemptHeaders();
}

PendingCastComponent::~PendingCastComponent() = default;

std::vector<std::vector<uint8_t>>
PendingCastComponent::TakeCorsExemptHeaders() {
  std::vector<std::vector<uint8_t>> headers;
  cors_exempt_headers_->swap(headers);
  return headers;
}

void PendingCastComponent::RequestCorsExemptHeaders() {
  params_.startup_context->svc()->Connect(
      cors_exempt_headers_provider_.NewRequest());

  cors_exempt_headers_provider_.set_error_handler([this](zx_status_t status) {
    if (status != ZX_ERR_PEER_CLOSED) {
      ZX_LOG(ERROR, status) << "CorsExemptHeaderProvider disconnected.";
      delegate_->CancelPendingComponent(this);
      return;
    }

    ZX_LOG(WARNING, status) << "CorsExemptHeaderProvider unsupported.";
    cors_exempt_headers_.emplace();  // Set an empty header list.
    MaybeLaunchComponent();
  });

  cors_exempt_headers_provider_->GetCorsExemptHeaderNames(
      [this](std::vector<std::vector<uint8_t>> header_names) {
        cors_exempt_headers_provider_.Unbind();
        cors_exempt_headers_ = header_names;
        MaybeLaunchComponent();
      });
}

void PendingCastComponent::OnApplicationConfigReceived(
    chromium::cast::ApplicationConfig application_config) {
  if (application_config.IsEmpty()) {
    DLOG(WARNING) << "No application config was found.";
    delegate_->CancelPendingComponent(this);
    return;
  }

  if (!application_config.has_web_url()) {
    DLOG(WARNING) << "Only web-based applications are supported.";
    delegate_->CancelPendingComponent(this);
    return;
  }

  if (!application_config.has_agent_url()) {
    DLOG(WARNING) << "No agent has been associated with this app.";
    delegate_->CancelPendingComponent(this);
    return;
  }

  params_.application_config = std::move(application_config);

  // Request custom API bindings from the component's Agent.
  params_.api_bindings_client = std::make_unique<ApiBindingsClient>(
      params_.agent_manager->ConnectToAgentService<chromium::cast::ApiBindings>(
          params_.application_config.agent_url()),
      base::BindOnce(&PendingCastComponent::OnApiBindingsInitialized,
                     base::Unretained(this)));

  // Request UrlRequestRewriteRulesProvider from the Agent.
  params_.agent_manager->ConnectToAgentService(
      params_.application_config.agent_url(),
      params_.url_rewrite_rules_provider.NewRequest());
  params_.url_rewrite_rules_provider.set_error_handler([this](
                                                           zx_status_t status) {
    if (status != ZX_ERR_PEER_CLOSED) {
      ZX_LOG(ERROR, status) << "UrlRequestRewriteRulesProvider disconnected.";
      delegate_->CancelPendingComponent(this);
      return;
    }
    ZX_DLOG(WARNING, status) << "UrlRequestRewriteRulesProvider unsupported.";
    params_.initial_url_rewrite_rules =
        std::vector<fuchsia::web::UrlRequestRewriteRule>();
    MaybeLaunchComponent();
  });
  params_.url_rewrite_rules_provider->GetUrlRequestRewriteRules(
      [this](std::vector<fuchsia::web::UrlRequestRewriteRule> rewrite_rules) {
        params_.initial_url_rewrite_rules.emplace(std::move(rewrite_rules));
        MaybeLaunchComponent();
      });

  // Connect to the component-specific ApplicationContext to retrieve the
  // media-session identifier assigned to this instance.
  application_context_ =
      params_.agent_manager
          ->ConnectToAgentService<chromium::cast::ApplicationContext>(
              params_.application_config.agent_url());
  application_context_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(ERROR, status) << "ApplicationContext disconnected.";
    delegate_->CancelPendingComponent(this);
  });
  application_context_->GetMediaSessionId([this](uint64_t session_id) {
    params_.media_session_id = session_id;
    MaybeLaunchComponent();
  });
}

void PendingCastComponent::OnApiBindingsInitialized() {
  if (params_.api_bindings_client->HasBindings())
    MaybeLaunchComponent();
  else
    delegate_->CancelPendingComponent(this);
}

void PendingCastComponent::MaybeLaunchComponent() {
  if (!params_.AreComplete() || !cors_exempt_headers_)
    return;

  // Clear the error handlers on InterfacePtr<>s before passing them, to avoid
  // user-after-free of |this|.
  params_.url_rewrite_rules_provider.set_error_handler(nullptr);

  delegate_->LaunchPendingComponent(this, std::move(params_));
}
