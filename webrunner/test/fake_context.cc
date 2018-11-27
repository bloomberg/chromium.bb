// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/test/fake_context.h"

#include "base/logging.h"

namespace webrunner {

FakeFrame::FakeFrame(fidl::InterfaceRequest<chromium::web::Frame> request)
    : binding_(this, std::move(request)) {
  binding_.set_error_handler([this](zx_status_t status) { delete this; });
}

FakeFrame::~FakeFrame() = default;

void FakeFrame::SetNavigationEventObserver(
    fidl::InterfaceHandle<chromium::web::NavigationEventObserver> observer) {
  observer_.Bind(std::move(observer));
  std::move(on_set_observer_callback_).Run();
}

void FakeFrame::NotImplemented_(const std::string& name) {
  NOTREACHED() << name;
}

FakeContext::FakeContext() = default;
FakeContext::~FakeContext() = default;

void FakeContext::CreateFrame(
    fidl::InterfaceRequest<chromium::web::Frame> frame_request) {
  FakeFrame* new_frame = new FakeFrame(std::move(frame_request));
  on_create_frame_callback_.Run(new_frame);

  // |new_frame| owns itself, so we intentionally leak the pointer.
}

void FakeContext::NotImplemented_(const std::string& name) {
  NOTREACHED() << name;
}

}  // namespace webrunner
