// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_VIDEO_CAPTURE_FORMAT_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_VIDEO_CAPTURE_FORMAT_H_

#include "media/capture/video_capture_types.h"
#include "ui/gfx/geometry/size.h"

namespace video_capture {

// Cpp equivalent of Mojo struct video_capture::mojom::VideoCaptureFormat.
struct VideoCaptureFormat {
  gfx::Size frame_size;
  float frame_rate;

  void ConvertToMediaVideoCaptureFormat(
      media::VideoCaptureFormat* target) const {
    target->frame_size = frame_size;
    target->frame_rate = frame_rate;
    target->pixel_format = media::PIXEL_FORMAT_I420;
    target->pixel_storage = media::PIXEL_STORAGE_CPU;
  }
};

// Cpp equivalent of Mojo struct video_capture::mojom::VideoCaptureSettings.
struct VideoCaptureSettings {
  VideoCaptureFormat format;
  media::ResolutionChangePolicy resolution_change_policy;
  media::PowerLineFrequency power_line_frequency;

  void ConvertToMediaVideoCaptureParams(
      media::VideoCaptureParams* target) const {
    format.ConvertToMediaVideoCaptureFormat(&(target->requested_format));
    target->resolution_change_policy = resolution_change_policy;
    target->power_line_frequency = power_line_frequency;
  }
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_CPP_VIDEO_CAPTURE_FORMAT_H_
