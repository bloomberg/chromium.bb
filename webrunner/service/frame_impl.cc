// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/service/frame_impl.h"

#include "base/logging.h"

namespace webrunner {

FrameImpl::FrameImpl(chromium::web::FrameObserverPtr observer)
    : observer_(std::move(observer)) {}

FrameImpl::~FrameImpl() = default;

void FrameImpl::CreateView(
    ::fidl::InterfaceRequest<fuchsia::ui::views_v1_token::ViewOwner> view_owner,
    ::fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> services) {
  NOTIMPLEMENTED();
}

void FrameImpl::GetNavigationController(
    ::fidl::InterfaceRequest<chromium::web::NavigationController> controller) {
  controller_bindings_.AddBinding(this, std::move(controller));
}

void FrameImpl::LoadUrl(
    ::fidl::StringPtr url,
    ::std::unique_ptr<chromium::web::LoadUrlParams> params) {
  NOTIMPLEMENTED() << "Loading URL " << *url;
}

void FrameImpl::GoBack() {
  NOTIMPLEMENTED();
}

void FrameImpl::GoForward() {
  NOTIMPLEMENTED();
}

void FrameImpl::Stop() {
  NOTIMPLEMENTED();
}

void FrameImpl::Reload() {
  NOTIMPLEMENTED();
}

void FrameImpl::GetVisibleEntry(GetVisibleEntryCallback callback) {
  NOTIMPLEMENTED();
  callback(nullptr);
}

}  // namespace webrunner
