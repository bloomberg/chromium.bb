// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_RECEIVER_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_RECEIVER_H_

#include <vector>

#include "media/capture/mojom/video_capture_types.mojom.h"
#include "media/mojo/interfaces/media_types.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/video_capture/public/mojom/receiver.mojom.h"
#include "services/video_capture/public/mojom/scoped_access_permission.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace video_capture {

class MockReceiver : public mojom::Receiver {
 public:
  MockReceiver();
  explicit MockReceiver(mojom::ReceiverRequest request);
  ~MockReceiver() override;

  void HoldAccessPermissions();
  void ReleaseAccessPermissions();

  // Use forwarding method to work around gmock not supporting move-only types.
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override;
  void OnFrameReadyInBuffer(
      int32_t buffer_id,
      int32_t frame_feedback_id,
      mojom::ScopedAccessPermissionPtr access_permission,
      media::mojom::VideoFrameInfoPtr frame_info) override;
  void OnBufferRetired(int32_t buffer_id) override;

  MOCK_METHOD2(DoOnNewBuffer,
               void(int32_t, media::mojom::VideoBufferHandlePtr*));
  MOCK_METHOD4(DoOnFrameReadyInBuffer,
               void(int32_t buffer_id,
                    int32_t frame_feedback_id,
                    mojom::ScopedAccessPermissionPtr*,
                    media::mojom::VideoFrameInfoPtr*));
  MOCK_METHOD1(DoOnBufferRetired, void(int32_t));
  MOCK_METHOD1(OnError, void(media::VideoCaptureError));
  MOCK_METHOD1(OnFrameDropped, void(media::VideoCaptureFrameDropReason));
  MOCK_METHOD1(OnLog, void(const std::string&));
  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD0(OnStartedUsingGpuDecode, void());
  MOCK_METHOD0(OnStopped, void());

 private:
  const mojo::Binding<mojom::Receiver> binding_;
  std::vector<int32_t> known_buffer_ids_;
  bool should_store_access_permissions_;
  std::vector<mojom::ScopedAccessPermissionPtr> access_permissions_;
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_MOCK_RECEIVER_H_
