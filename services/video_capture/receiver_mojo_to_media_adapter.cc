// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/receiver_mojo_to_media_adapter.h"

namespace video_capture {

ReceiverMojoToMediaAdapter::ReceiverMojoToMediaAdapter(
    mojom::ReceiverPtr receiver)
    : receiver_(std::move(receiver)) {}

ReceiverMojoToMediaAdapter::~ReceiverMojoToMediaAdapter() = default;

void ReceiverMojoToMediaAdapter::OnNewBufferHandle(
    int buffer_id,
    std::unique_ptr<media::VideoCaptureDevice::Client::Buffer::HandleProvider>
        handle_provider) {
  NOTIMPLEMENTED();
}

void ReceiverMojoToMediaAdapter::OnFrameReadyInBuffer(
    int buffer_id,
    int frame_feedback_id,
    std::unique_ptr<
        media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
        buffer_usage_reservation,
    media::mojom::VideoFrameInfoPtr frame_info) {
  NOTIMPLEMENTED();
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

void ReceiverMojoToMediaAdapter::OnBufferRetired(int buffer_id) {
  NOTIMPLEMENTED();
}

}  // namespace video_capture
