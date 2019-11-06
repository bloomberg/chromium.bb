// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/cpp/mock_video_source.h"

namespace video_capture {

MockVideoSource::MockVideoSource() = default;

MockVideoSource::~MockVideoSource() = default;

void MockVideoSource::CreatePushSubscription(
    video_capture::mojom::ReceiverPtr subscriber,
    const media::VideoCaptureParams& requested_settings,
    bool force_reopen_with_new_settings,
    video_capture::mojom::PushVideoStreamSubscriptionRequest subscription,
    CreatePushSubscriptionCallback callback) {
  DoCreatePushSubscription(subscriber, requested_settings,
                           force_reopen_with_new_settings, subscription,
                           callback);
}

}  // namespace video_capture
