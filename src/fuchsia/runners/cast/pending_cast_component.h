// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_PENDING_CAST_COMPONENT_H_
#define FUCHSIA_RUNNERS_CAST_PENDING_CAST_COMPONENT_H_

#include <chromium/cast/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>
#include <memory>

#include "base/strings/string_piece.h"
#include "fuchsia/runners/cast/cast_component.h"

namespace base {
namespace fuchsia {
class StartupContext;
}
}  // namespace base

// Manages asynchronous retrieval of parameters required to launch the specified
// Cast |app_id|.
class PendingCastComponent {
 public:
  // Implemented by the PendingCastComponent's owner, to actually launch the
  // component, or cancel it.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called to launch the component with |params|. The Delegate should
    // clean up resources associated with |component| and delete it.
    virtual void LaunchPendingComponent(PendingCastComponent* component,
                                        CastComponent::Params params) = 0;

    // Called if an error occurs, to have the Delegate clean up resources
    // associated with |component|, and delete it.
    virtual void CancelPendingComponent(PendingCastComponent* component) = 0;
  };

  PendingCastComponent(
      Delegate* delegate,
      std::unique_ptr<base::fuchsia::StartupContext> startup_context,
      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
          controller_request,
      base::StringPiece app_id);
  ~PendingCastComponent();

  PendingCastComponent(const PendingCastComponent&) = delete;
  PendingCastComponent& operator=(const PendingCastComponent&) = delete;

  // Returns the list of headers which will be exempted from CORS checks.
  std::vector<std::vector<uint8_t>> TakeCorsExemptHeaders();

 private:
  void RequestCorsExemptHeaders();

  // Handlers for completing initialization of some |params_| fields.
  void OnApplicationConfigReceived(
      chromium::cast::ApplicationConfig app_config);
  void OnApiBindingsInitialized();
  void OnCorsHeadersReceived(std::vector<std::vector<uint8_t>> header_names);

  // Passes |params_| to |delegate_| if they are complete, to use to launch the
  // component. |this| will be deleted before the call returns in that case.
  // Has no effect if |params_| are not yet complete.
  void MaybeLaunchComponent();

  // Reference to the Delegate which manages |this|.
  Delegate* const delegate_;

  // Parameters required to construct the CastComponent.
  CastComponent::Params params_;

  // The list of HTTP headers to exempt from CORS checked. The value is used
  // when a Context is created to host this Component.
  base::Optional<std::vector<std::vector<uint8_t>>> cors_exempt_headers_;

  // Used to receive the media session Id and ApplicationConfig.
  chromium::cast::ApplicationContextPtr application_context_;
  chromium::cast::ApplicationConfigManagerPtr application_config_manager_;
  chromium::cast::CorsExemptHeaderProviderPtr cors_exempt_headers_provider_;
};

#endif  // FUCHSIA_RUNNERS_CAST_PENDING_CAST_COMPONENT_H_
