// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/mock_video_capture_device_client.h"

namespace video_capture {

MockVideoCaptureDeviceClient::MockVideoCaptureDeviceClient(
    mojom::VideoCaptureDeviceClientRequest request)
    : binding_(this, std::move(request)) {}

MockVideoCaptureDeviceClient::~MockVideoCaptureDeviceClient() = default;

void MockVideoCaptureDeviceClient::OnFrameAvailable(
    media::mojom::VideoFramePtr frame) {
  OnFrameAvailablePtr(&frame);
}

}  // namespace video_capture
