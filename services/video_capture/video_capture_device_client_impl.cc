// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "services/video_capture/video_capture_device_client_impl.h"

namespace video_capture {

void VideoCaptureDeviceClientImpl::OnFrameAvailable(
    media::mojom::VideoFramePtr frame) {
  NOTIMPLEMENTED();
}

void VideoCaptureDeviceClientImpl::OnError(const std::string& error) {
  NOTIMPLEMENTED();
}

}  // namespace video_capture
