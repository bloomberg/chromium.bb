// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_MOCK_VIDEO_CAPTURE_DEVICE_CLIENT_H_
#define SERVICES_VIDEO_CAPTURE_MOCK_VIDEO_CAPTURE_DEVICE_CLIENT_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "services/video_capture/public/interfaces/video_capture_device_client.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockVideoCaptureDeviceClient : public mojom::VideoCaptureDeviceClient {
 public:
  MockVideoCaptureDeviceClient(mojom::VideoCaptureDeviceClientRequest request);
  ~MockVideoCaptureDeviceClient() override;

  // Use forwarding method to work around gmock not supporting move-only types.
  void OnFrameAvailable(media::mojom::VideoFramePtr frame) override;

  MOCK_METHOD1(OnFrameAvailablePtr, void(media::mojom::VideoFramePtr* frame));
  MOCK_METHOD1(OnError, void(const std::string&));

 private:
  const mojo::Binding<mojom::VideoCaptureDeviceClient> binding_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_MOCK_VIDEO_CAPTURE_DEVICE_CLIENT_H_
