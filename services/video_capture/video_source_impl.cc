// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/video_source_impl.h"

namespace video_capture {

VideoSourceImpl::VideoSourceImpl(
    mojom::DeviceFactory* device_factory,
    const std::string& device_id,
    base::RepeatingClosure on_last_binding_closed_cb)
    : device_factory_(device_factory),
      device_id_(device_id),
      on_last_binding_closed_cb_(std::move(on_last_binding_closed_cb)),
      weak_factory_(this) {
  CHECK(device_factory_);
  CHECK_GE(device_id_.size(), 0u);
  bindings_.set_connection_error_handler(base::BindRepeating(
      &VideoSourceImpl::OnClientDisconnected, base::Unretained(this)));
}

VideoSourceImpl::~VideoSourceImpl() {
  bindings_.set_connection_error_handler(base::DoNothing());
}

void VideoSourceImpl::AddToBindingSet(mojom::VideoSourceRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void VideoSourceImpl::OnClientDisconnected() {
  if (bindings_.empty()) {
    // Note: Invoking this callback may synchronously trigger the destruction of
    // |this|, so no more member access should be done after it.
    on_last_binding_closed_cb_.Run();
  }
}

}  // namespace video_capture
