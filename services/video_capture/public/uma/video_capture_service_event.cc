// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/uma/video_capture_service_event.h"

#include "base/metrics/histogram_macros.h"

namespace video_capture {
namespace uma {

void LogVideoCaptureServiceEvent(VideoCaptureServiceEvent event) {
  UMA_HISTOGRAM_ENUMERATION("Media.VideoCaptureService.Event", event,
                            NUM_VIDEO_CAPTURE_SERVICE_EVENT);
}

void LogDurationFromLastConnectToClosingConnection(base::TimeDelta duration) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Media.VideoCaptureService.DurationFromLastConnectToClosingConnection",
      duration, base::TimeDelta(), base::TimeDelta::FromDays(7), 50);
}

void LogDurationFromLastConnectToConnectionLost(base::TimeDelta duration) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "Media.VideoCaptureService.DurationFromLastConnectToConnectionLost",
      duration, base::TimeDelta(), base::TimeDelta::FromDays(7), 50);
}

void LogDurationUntilReconnect(base::TimeDelta duration) {
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.VideoCaptureService.DurationUntilReconnect",
                             duration, base::TimeDelta(),
                             base::TimeDelta::FromDays(7), 50);
}

}  // namespace uma
}  // namespace video_capture
