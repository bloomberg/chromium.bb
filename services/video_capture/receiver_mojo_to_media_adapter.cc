// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/receiver_mojo_to_media_adapter.h"

#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/mojo_shared_buffer_video_frame.h"

namespace video_capture {

ReceiverMojoToMediaAdapter::ReceiverMojoToMediaAdapter(
    mojom::ReceiverPtr receiver)
    : receiver_(std::move(receiver)) {}

ReceiverMojoToMediaAdapter::~ReceiverMojoToMediaAdapter() = default;

void ReceiverMojoToMediaAdapter::OnIncomingCapturedVideoFrame(
    media::VideoCaptureDevice::Client::Buffer buffer,
    scoped_refptr<media::VideoFrame> frame) {
  NOTIMPLEMENTED();
}

void ReceiverMojoToMediaAdapter::OnError() {
  receiver_->OnError();
}

void ReceiverMojoToMediaAdapter::OnLog(const std::string& message) {
  receiver_->OnLog(message);
}

void ReceiverMojoToMediaAdapter::OnBufferRetired(int buffer_id) {
  NOTIMPLEMENTED();
}

}  // namespace video_capture
