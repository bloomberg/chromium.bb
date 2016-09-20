// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/mock_video_capture_device_impl.h"

namespace video_capture {

MockVideoCaptureDeviceImpl::MockVideoCaptureDeviceImpl(
    mojom::MockVideoCaptureDeviceRequest request)
    : binding_(this, std::move(request)) {}

MockVideoCaptureDeviceImpl::~MockVideoCaptureDeviceImpl() = default;

void MockVideoCaptureDeviceImpl::AllocateAndStart(
    mojom::MockDeviceClientPtr client) {
  AllocateAndStartPtr(&client);
}

}  // namespace video_capture
