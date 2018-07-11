// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/app/component_controller_impl.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <utility>
#include <vector>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory.h"
#include "base/logging.h"
#include "webrunner/app/web_content_runner.h"
#include "webrunner/fidl/chromium/web/cpp/fidl.h"

namespace webrunner {

ComponentControllerImpl::ComponentControllerImpl(
    WebContentRunner* runner,
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    ::fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request)
    : runner_(runner),
      controller_binding_(this),
      frame_observer_binding_(this) {
  DCHECK(runner);
  BindToRequest(std::move(package), std::move(startup_info),
                std::move(controller_request));
}

ComponentControllerImpl::~ComponentControllerImpl() {
  for (WaitCallback& next_callback : termination_wait_callbacks_) {
    next_callback(did_terminate_abnormally_ ? 1 : 0);
  }
}

void ComponentControllerImpl::BindToRequest(
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    ::fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  DCHECK(!service_directory_);
  DCHECK(!view_provider_binding_);

  url_ = GURL(*package.resolved_url);
  if (!url_.is_valid()) {
    LOG(ERROR) << "Rejected invalid URL: " << url_;

    // By now |this| has been destroyed by WebContentRunner, so return
    // immediately.
    return;
  }

  if (controller_request.is_valid()) {
    controller_binding_.Bind(std::move(controller_request));
    controller_binding_.set_error_handler(
        fit::bind_member(this, &ComponentControllerImpl::Kill));
  }

  runner_->context()->CreateFrame(frame_observer_binding_.NewBinding(),
                                  frame_.NewRequest());
  frame_->GetNavigationController(navigation_controller_.NewRequest());
  navigation_controller_->LoadUrl(url_.spec(), nullptr);

  // Publish a ViewProvider service.
  service_directory_ = std::make_unique<base::fuchsia::ServiceDirectory>(
      std::move(startup_info.launch_info.directory_request));
  view_provider_binding_ = std::make_unique<
      base::fuchsia::ScopedServiceBinding<fuchsia::ui::views_v1::ViewProvider>>(
      service_directory_.get(), this);
}

void ComponentControllerImpl::Kill() {
  did_terminate_abnormally_ = true;
  runner_->DestroyComponent(this);
}

void ComponentControllerImpl::Detach() {
  controller_binding_.set_error_handler(nullptr);
}

void ComponentControllerImpl::Wait(WaitCallback callback) {
  termination_wait_callbacks_.push_back(std::move(callback));
}

void ComponentControllerImpl::OnNavigationStateChanged(
    chromium::web::NavigationStateChangeDetails change,
    OnNavigationStateChangedCallback callback) {}

void ComponentControllerImpl::CreateView(
    ::fidl::InterfaceRequest<fuchsia::ui::views_v1_token::ViewOwner> view_owner,
    ::fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> services) {
  DCHECK(frame_);
  DCHECK(!view_bound_);

  frame_->CreateView(std::move(view_owner), std::move(services));
  view_bound_ = true;
}

}  // namespace webrunner
