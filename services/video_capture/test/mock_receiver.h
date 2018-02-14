// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_TEST_MOCK_RECEIVER_H_
#define SERVICES_VIDEO_CAPTURE_TEST_MOCK_RECEIVER_H_

#include "media/mojo/interfaces/media_types.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/video_capture/public/mojom/receiver.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockReceiver : public mojom::Receiver {
 public:
  MockReceiver(mojom::ReceiverRequest request);
  ~MockReceiver() override;

  // Use forwarding method to work around gmock not supporting move-only types.
  void OnNewBufferHandle(int32_t buffer_id,
                         mojo::ScopedSharedBufferHandle buffer_handle) override;
  void OnFrameReadyInBuffer(
      int32_t buffer_id,
      int32_t frame_feedback_id,
      mojom::ScopedAccessPermissionPtr access_permission,
      media::mojom::VideoFrameInfoPtr frame_info) override;

  MOCK_METHOD2(DoOnNewBufferHandle,
               void(int32_t, mojo::ScopedSharedBufferHandle*));
  MOCK_METHOD4(DoOnFrameReadyInBuffer,
               void(int32_t buffer_id,
                    int32_t frame_feedback_id,
                    mojom::ScopedAccessPermissionPtr*,
                    media::mojom::VideoFrameInfoPtr*));
  MOCK_METHOD1(OnBufferRetired, void(int32_t));
  MOCK_METHOD0(OnError, void());
  MOCK_METHOD1(OnLog, void(const std::string&));
  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStartedUsingGpuDecode, void());

 private:
  const mojo::Binding<mojom::Receiver> binding_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_TEST_MOCK_RECEIVER_H_
