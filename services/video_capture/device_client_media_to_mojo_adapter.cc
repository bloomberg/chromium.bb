// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/device_client_media_to_mojo_adapter.h"

namespace video_capture {

DeviceClientMediaToMojoAdapter::DeviceClientMediaToMojoAdapter(
    std::unique_ptr<media::VideoCaptureDevice::Client> client)
    : client_(std::move(client)) {}

DeviceClientMediaToMojoAdapter::~DeviceClientMediaToMojoAdapter() = default;

}  // namespace video_capture
