// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "services/video_capture/video_capture_device_impl.h"

namespace video_capture {

void VideoCaptureDeviceImpl::Start(
    mojom::VideoCaptureFormatPtr requested_format,
    mojom::ResolutionChangePolicy resolution_change_policy,
    mojom::PowerLineFrequency power_line_frequency,
    mojom::VideoCaptureDeviceClientPtr client) {
  NOTIMPLEMENTED();
}

}  // namespace video_capture
