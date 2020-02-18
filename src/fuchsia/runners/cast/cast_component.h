// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_CAST_COMPONENT_H_
#define FUCHSIA_RUNNERS_CAST_CAST_COMPONENT_H_

#include <lib/fidl/cpp/binding.h>
#include <memory>

#include "base/fuchsia/service_directory.h"
#include "base/fuchsia/startup_context.h"
#include "base/optional.h"
#include "fuchsia/base/agent_manager.h"
#include "fuchsia/runners/cast/api_bindings_client.h"
#include "fuchsia/runners/cast/application_controller_impl.h"
#include "fuchsia/runners/cast/named_message_port_connector.h"
#include "fuchsia/runners/cast/touch_input_bindings.h"
#include "fuchsia/runners/common/web_component.h"

class CastRunner;

// A specialization of WebComponent which adds Cast-specific services.
class CastComponent : public WebComponent,
                      public fuchsia::web::NavigationEventListener {
 public:
  struct CastComponentParams {
    CastComponentParams();
    CastComponentParams(CastComponentParams&&);
    ~CastComponentParams();

    chromium::cast::ApplicationConfigManagerPtr app_config_manager;
    std::unique_ptr<base::fuchsia::StartupContext> startup_context;
    std::unique_ptr<cr_fuchsia::AgentManager> agent_manager;
    std::unique_ptr<ApiBindingsClient> api_bindings_client;
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request;
    chromium::cast::ApplicationConfig app_config;
    fuchsia::web::AdditionalHeadersProviderPtr headers_provider;
    base::Optional<std::vector<fuchsia::net::http::Header>> headers;
  };

  CastComponent(CastRunner* runner, CastComponentParams params);
  ~CastComponent() override;

 private:
  // TODO(crbug.com/953958): Remove this.
  void InitializeCastPlatformBindings();

  // WebComponent overrides.
  void DestroyComponent(int termination_exit_code,
                        fuchsia::sys::TerminationReason reason) override;

  // fuchsia::web::NavigationEventListener implementation.
  // Triggers the injection of API channels into the page content.
  void OnNavigationStateChanged(
      fuchsia::web::NavigationState change,
      OnNavigationStateChangedCallback callback) override;

  std::unique_ptr<cr_fuchsia::AgentManager> agent_manager_;
  chromium::cast::ApplicationConfig application_config_;

  bool constructor_active_ = false;
  TouchInputPolicy touch_input_policy_;
  NamedMessagePortConnector connector_;
  std::unique_ptr<TouchInputBindings> touch_input_;
  std::unique_ptr<ApiBindingsClient> api_bindings_client_;
  std::unique_ptr<ApplicationControllerImpl> application_controller_;

  fidl::Binding<fuchsia::web::NavigationEventListener>
      navigation_listener_binding_;

  DISALLOW_COPY_AND_ASSIGN(CastComponent);
};

#endif  // FUCHSIA_RUNNERS_CAST_CAST_COMPONENT_H_
