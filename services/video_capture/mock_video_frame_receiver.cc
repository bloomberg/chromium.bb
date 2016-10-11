// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/mock_video_frame_receiver.h"

namespace video_capture {

MockVideoFrameReceiver::MockVideoFrameReceiver(
    mojom::VideoFrameReceiverRequest request)
    : binding_(this, std::move(request)) {}

MockVideoFrameReceiver::~MockVideoFrameReceiver() = default;

void MockVideoFrameReceiver::OnIncomingCapturedVideoFrame(
    media::mojom::VideoFramePtr frame) {
  OnIncomingCapturedVideoFramePtr(&frame);
}

}  // namespace video_capture
