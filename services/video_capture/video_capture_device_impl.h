// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_IMPL_H_

#include "services/video_capture/public/interfaces/video_capture_device.mojom.h"

namespace video_capture {

// Implementation of mojom::VideoCaptureDeviceProxy backed by a given instance
// of media::VideoCaptureDevice.
class VideoCaptureDeviceImpl : public mojom::VideoCaptureDevice {
 public:
  // mojom::VideoCaptureDeviceProxy:
  void Start(mojom::VideoCaptureFormatPtr requested_format,
             mojom::ResolutionChangePolicy resolution_change_policy,
             mojom::PowerLineFrequency power_line_frequency,
             mojom::VideoCaptureDeviceClientPtr client) override;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_IMPL_H_
