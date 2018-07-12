// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webrunner/service/context_impl.h"

#include <memory>
#include <utility>

#include "webrunner/service/frame_impl.h"

namespace webrunner {

ContextImpl::ContextImpl() = default;

ContextImpl::~ContextImpl() = default;

void ContextImpl::CreateFrame(
    ::fidl::InterfaceHandle<chromium::web::FrameObserver> observer,
    ::fidl::InterfaceRequest<chromium::web::Frame> frame_request) {
  std::unique_ptr<chromium::web::Frame> frame =
      std::make_unique<FrameImpl>(observer.Bind());
  frame_bindings_.AddBinding(std::move(frame), std::move(frame_request));
}

}  // namespace webrunner
