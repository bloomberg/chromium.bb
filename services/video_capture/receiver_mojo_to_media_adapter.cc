// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/receiver_mojo_to_media_adapter.h"

#include "mojo/public/cpp/bindings/strong_binding.h"

namespace {

class ScopedAccessPermissionMediaToMojoAdapter
    : public video_capture::mojom::ScopedAccessPermission {
 public:
  ScopedAccessPermissionMediaToMojoAdapter(
      std::unique_ptr<
          media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
          access_permission)
      : access_permission_(std::move(access_permission)) {}

 private:
  std::unique_ptr<
      media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
      access_permission_;
};

}  // anonymous namespace

namespace video_capture {

ReceiverMojoToMediaAdapter::ReceiverMojoToMediaAdapter(
    mojom::ReceiverPtr receiver)
    : receiver_(std::move(receiver)) {}

ReceiverMojoToMediaAdapter::~ReceiverMojoToMediaAdapter() = default;

void ReceiverMojoToMediaAdapter::OnNewBufferHandle(
    int buffer_id,
    std::unique_ptr<media::VideoCaptureDevice::Client::Buffer::HandleProvider>
        handle_provider) {
  receiver_->OnNewBufferHandle(
      buffer_id, handle_provider->GetHandleForInterProcessTransit());
}

void ReceiverMojoToMediaAdapter::OnFrameReadyInBuffer(
    int buffer_id,
    int frame_feedback_id,
    std::unique_ptr<
        media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
        access_permission,
    media::mojom::VideoFrameInfoPtr frame_info) {
  mojom::ScopedAccessPermissionPtr access_permission_proxy;
  mojo::MakeStrongBinding<mojom::ScopedAccessPermission>(
      base::MakeUnique<ScopedAccessPermissionMediaToMojoAdapter>(
          std::move(access_permission)),
      mojo::MakeRequest(&access_permission_proxy));
  receiver_->OnFrameReadyInBuffer(buffer_id, frame_feedback_id,
                                  std::move(access_permission_proxy),
                                  std::move(frame_info));
}

void ReceiverMojoToMediaAdapter::OnBufferRetired(int buffer_id) {
  receiver_->OnBufferRetired(buffer_id);
}

void ReceiverMojoToMediaAdapter::OnError() {
  receiver_->OnError();
}

void ReceiverMojoToMediaAdapter::OnLog(const std::string& message) {
  receiver_->OnLog(message);
}

void ReceiverMojoToMediaAdapter::OnStarted() {
  receiver_->OnStarted();
}

void ReceiverMojoToMediaAdapter::OnStartedUsingGpuDecode() {
  receiver_->OnStartedUsingGpuDecode();
}

}  // namespace video_capture
