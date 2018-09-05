// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/app/component_controller_impl.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/fit/function.h>
#include <utility>
#include <vector>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory.h"
#include "base/logging.h"
#include "webrunner/app/web_content_runner.h"
#include "webrunner/fidl/chromium/web/cpp/fidl.h"

namespace webrunner {

// static
std::unique_ptr<ComponentControllerImpl>
ComponentControllerImpl::CreateForRequest(
    WebContentRunner* runner,
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  std::unique_ptr<ComponentControllerImpl> result{
      new ComponentControllerImpl(runner)};
  if (!result->BindToRequest(std::move(package), std::move(startup_info),
                             std::move(controller_request))) {
    return nullptr;
  }
  return result;
}

ComponentControllerImpl::ComponentControllerImpl(WebContentRunner* runner)
    : runner_(runner), controller_binding_(this) {
  DCHECK(runner);
}

ComponentControllerImpl::~ComponentControllerImpl() {
  // Send process termination details to the client.
  controller_binding_.events().OnTerminated(termination_exit_code_,
                                            termination_reason_);
}

bool ComponentControllerImpl::BindToRequest(
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  DCHECK(!service_directory_);
  DCHECK(!view_provider_binding_);

  url_ = GURL(*package.resolved_url);
  if (!url_.is_valid()) {
    LOG(ERROR) << "Rejected invalid URL: " << url_;
    return false;
  }

  if (controller_request.is_valid()) {
    controller_binding_.Bind(std::move(controller_request));
    controller_binding_.set_error_handler([this] {
      // Signal graceful process termination.
      RequestTermination(0, fuchsia::sys::TerminationReason::EXITED);
    });
  }

  runner_->context()->CreateFrame(frame_.NewRequest());
  frame_->GetNavigationController(navigation_controller_.NewRequest());
  navigation_controller_->LoadUrl(url_.spec(), nullptr);

  // Create ServiceDirectory for the component and publish ViewProvider
  // interface. ViewProvider will be used by the caller to create a view for the
  // Frame. Note that these two operations must be done on the same/
  // AsyncDispatcher in order to ensure that ServiceDirectory doesn't process
  // incoming service requests before the service is published.
  service_directory_ = std::make_unique<base::fuchsia::ServiceDirectory>(
      std::move(startup_info.launch_info.directory_request));
  view_provider_binding_ = std::make_unique<
      base::fuchsia::ScopedServiceBinding<fuchsia::ui::viewsv1::ViewProvider>>(
      service_directory_.get(), this);

  return true;
}

void ComponentControllerImpl::Kill() {
  // Signal abnormal process termination.
  RequestTermination(1, fuchsia::sys::TerminationReason::RUNNER_TERMINATED);
}

void ComponentControllerImpl::Detach() {
  controller_binding_.set_error_handler(nullptr);
}

void ComponentControllerImpl::CreateView(
    fidl::InterfaceRequest<fuchsia::ui::viewsv1token::ViewOwner> view_owner,
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> services) {
  DCHECK(frame_);
  DCHECK(!view_is_bound_);

  frame_->CreateView(std::move(view_owner), std::move(services));
  view_is_bound_ = true;
}

void ComponentControllerImpl::RequestTermination(
    int termination_exit_code,
    fuchsia::sys::TerminationReason reason) {
  termination_reason_ = reason;
  termination_exit_code_ = termination_exit_code;
  runner_->DestroyComponent(this);
}

}  // namespace webrunner
