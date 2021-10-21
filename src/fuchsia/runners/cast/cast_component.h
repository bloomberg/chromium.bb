// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_CAST_COMPONENT_H_
#define FUCHSIA_RUNNERS_CAST_CAST_COMPONENT_H_

#include <fuchsia/camera3/cpp/fidl.h>
#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/fuchsia/startup_context.h"
#include "base/gtest_prod_util.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_fuchsia.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia/runners/cast/api_bindings_client.h"
#include "fuchsia/runners/cast/application_controller_impl.h"
#include "fuchsia/runners/cast/named_message_port_connector_fuchsia.h"
#include "fuchsia/runners/common/web_component.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cr_fuchsia {
class AgentManager;
}

FORWARD_DECLARE_TEST(HeadlessCastRunnerIntegrationTest, Headless);

// A specialization of WebComponent which adds Cast-specific services.
class CastComponent final : public WebComponent,
                            public base::MessagePumpFuchsia::ZxHandleWatcher {
 public:
  struct Params {
    Params();
    Params(Params&&);
    ~Params();

    // Returns true if all parameters required for component launch have
    // been initialized.
    bool AreComplete() const;

    // Parameters populated directly from the StartComponent() arguments.
    std::unique_ptr<base::StartupContext> startup_context;
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request;

    // Parameters initialized synchronously.
    std::unique_ptr<cr_fuchsia::AgentManager> agent_manager;
    chromium::cast::UrlRequestRewriteRulesProviderPtr
        url_rewrite_rules_provider;

    // Parameters asynchronously initialized by PendingCastComponent.
    std::unique_ptr<ApiBindingsClient> api_bindings_client;
    chromium::cast::ApplicationConfig application_config;
    fidl::InterfaceHandle<chromium::cast::ApplicationContext>
        application_context;
    absl::optional<std::vector<fuchsia::web::UrlRequestRewriteRule>>
        initial_url_rewrite_rules;
    absl::optional<uint64_t> media_session_id;
  };

  // See WebComponent documentation for details of |debug_name| and |runner|.
  // |params| provides the Cast application configuration to use.
  // |is_headless| must match the headless setting of the specfied |runner|, to
  //   have CreateView() operations trigger enabling & disabling of off-screen
  //   rendering.
  CastComponent(base::StringPiece debug_name,
                WebContentRunner* runner,
                Params params,
                bool is_headless);

  CastComponent(const CastComponent&) = delete;
  CastComponent& operator=(const CastComponent&) = delete;

  ~CastComponent() override;

  void SetOnDestroyedCallback(base::OnceClosure on_destroyed);

  void ConnectMetricsRecorder(
      fidl::InterfaceRequest<fuchsia::legacymetrics::MetricsRecorder> request);
  void ConnectAudio(fidl::InterfaceRequest<fuchsia::media::Audio> request);
  void ConnectDeviceWatcher(
      fidl::InterfaceRequest<fuchsia::camera3::DeviceWatcher> request);

  bool HasWebPermission(fuchsia::web::PermissionType permission_type) const;

  const std::string& agent_url() const {
    return application_config_.agent_url();
  }

  // WebComponent overrides.
  void StartComponent() override;
  void DestroyComponent(int64_t termination_exit_code,
                        fuchsia::sys::TerminationReason reason) override;

 private:
  void OnRewriteRulesReceived(
      std::vector<fuchsia::web::UrlRequestRewriteRule> url_rewrite_rules);

  // fuchsia::web::NavigationEventListener implementation.
  // Triggers the injection of API channels into the page content.
  void OnNavigationStateChanged(
      fuchsia::web::NavigationState change,
      OnNavigationStateChangedCallback callback) override;

  // fuchsia::ui::app::ViewProvider implementation.
  void CreateView(
      zx::eventpair view_token,
      fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
      fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services)
      override;
  void CreateViewWithViewRef(zx::eventpair view_token,
                             fuchsia::ui::views::ViewRefControl control_ref,
                             fuchsia::ui::views::ViewRef view_ref) override;

  // base::MessagePumpFuchsia::ZxHandleWatcher implementation.
  // Called when the headless "view" token is disconnected.
  void OnZxHandleSignalled(zx_handle_t handle, zx_signals_t signals) override;

  const bool is_headless_;
  base::OnceClosure on_destroyed_;

  std::unique_ptr<cr_fuchsia::AgentManager> agent_manager_;
  chromium::cast::ApplicationConfig application_config_;
  chromium::cast::UrlRequestRewriteRulesProviderPtr url_rewrite_rules_provider_;
  std::vector<fuchsia::web::UrlRequestRewriteRule> initial_url_rewrite_rules_;

  bool constructor_active_ = false;
  std::unique_ptr<NamedMessagePortConnectorFuchsia> connector_;
  std::unique_ptr<ApiBindingsClient> api_bindings_client_;
  std::unique_ptr<ApplicationControllerImpl> application_controller_;
  chromium::cast::ApplicationContextPtr application_context_;
  uint64_t media_session_id_ = 0;
  zx::eventpair headless_view_token_;
  base::MessagePumpForIO::ZxHandleWatchController headless_disconnect_watch_;
};

#endif  // FUCHSIA_RUNNERS_CAST_CAST_COMPONENT_H_
