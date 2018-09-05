// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_APP_COMPONENT_CONTROLLER_IMPL_H_
#define WEBRUNNER_APP_COMPONENT_CONTROLLER_IMPL_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/binding_set.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory.h"
#include "base/logging.h"
#include "url/gurl.h"
#include "webrunner/fidl/chromium/web/cpp/fidl.h"

namespace webrunner {

class WebContentRunner;

// Manages the resources and service bindings for a Runner Component creation
// request. Each ComponentControllerImpl instance manages its own
// chromium::web::Frame.
class ComponentControllerImpl : public fuchsia::sys::ComponentController,
                                public fuchsia::ui::viewsv1::ViewProvider {
 public:
  ~ComponentControllerImpl() override;

  // |runner| must outlive the returned object (normally it owns all
  // ComponentControllerImpl). It's used by this class to get web::Context
  // interface.
  static std::unique_ptr<ComponentControllerImpl> CreateForRequest(
      WebContentRunner* runner,
      fuchsia::sys::Package package,
      fuchsia::sys::StartupInfo startup_info,
      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
          controller_request);

  // fuchsia::sys::ComponentController implementation.
  void Kill() override;
  void Detach() override;

  // fuchsia::ui::viewsv1::ViewProvider implementation.
  void CreateView(
      fidl::InterfaceRequest<::fuchsia::ui::viewsv1token::ViewOwner> view_owner,
      fidl::InterfaceRequest<::fuchsia::sys::ServiceProvider> services)
      override;

 private:
  explicit ComponentControllerImpl(WebContentRunner* runner);

  // Registers the termination reason for this Component and requests its
  // termination from the parent WebContentRunner.
  void RequestTermination(int termination_exit_code,
                          fuchsia::sys::TerminationReason reason);

  // Binds |this| to a Runner::StartComponent() call. Returns false on failure
  // (e.g. when the URL in |startup_info| is invalid).
  bool BindToRequest(fuchsia::sys::Package package,
                     fuchsia::sys::StartupInfo startup_info,
                     fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                         controller_request);

  GURL url_;
  WebContentRunner* runner_ = nullptr;

  chromium::web::FramePtr frame_;
  chromium::web::NavigationControllerPtr navigation_controller_;

  fidl::Binding<fuchsia::sys::ComponentController> controller_binding_;

  // Objects used for binding and exporting the ViewProvider service.
  std::unique_ptr<base::fuchsia::ServiceDirectory> service_directory_;
  std::unique_ptr<
      base::fuchsia::ScopedServiceBinding<fuchsia::ui::viewsv1::ViewProvider>>
      view_provider_binding_;

  // Termination reason and exit-code to be reported via the
  // sys::ComponentController::OnTerminated event.
  fuchsia::sys::TerminationReason termination_reason_ =
      fuchsia::sys::TerminationReason::UNKNOWN;
  int termination_exit_code_ = 0;

  bool view_is_bound_ = false;

  DISALLOW_COPY_AND_ASSIGN(ComponentControllerImpl);
};

}  // namespace webrunner

#endif  // WEBRUNNER_APP_COMPONENT_CONTROLLER_IMPL_H_
