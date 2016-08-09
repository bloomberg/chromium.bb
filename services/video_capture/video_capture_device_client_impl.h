// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_CLIENT_IMPL_H_
#define SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_CLIENT_IMPL_H_

#include "services/video_capture/public/interfaces/video_capture_device_client.mojom.h"

namespace video_capture {

// Implementation of the VideoCaptureDeviceClient Mojo interface.
class VideoCaptureDeviceClientImpl : public mojom::VideoCaptureDeviceClient {
 public:
  // mojom::VideoCaptureDeviceClient:
  void OnFrameAvailable(media::mojom::VideoFramePtr frame) override;
  void OnError(const std::string& error) override;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_VIDEO_CAPTURE_DEVICE_CLIENT_IMPL_H_
