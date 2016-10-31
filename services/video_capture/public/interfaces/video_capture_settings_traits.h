// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_VIDEO_CAPTURE_SETTINGS_TRAITS_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_VIDEO_CAPTURE_SETTINGS_TRAITS_H_

#include "media/capture/video_capture_types.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "services/video_capture/public/interfaces/video_capture_device_proxy.mojom.h"

namespace mojo {

template <>
struct StructTraits<video_capture::mojom::VideoCaptureFormatDataView,
                    video_capture::VideoCaptureFormat> {
  static const gfx::Size& frame_size(
      const video_capture::VideoCaptureFormat& input) {
    return input.frame_size;
  }

  static float frame_rate(const video_capture::VideoCaptureFormat& input) {
    return input.frame_rate;
  }

  static bool Read(video_capture::mojom::VideoCaptureFormatDataView data,
                   video_capture::VideoCaptureFormat* out);
};

template <>
struct StructTraits<video_capture::mojom::VideoCaptureSettingsDataView,
                    video_capture::VideoCaptureSettings> {
  static const video_capture::VideoCaptureFormat& format(
      const video_capture::VideoCaptureSettings& input) {
    return input.format;
  }

  static media::ResolutionChangePolicy resolution_change_policy(
      const video_capture::VideoCaptureSettings& input) {
    return input.resolution_change_policy;
  }

  static media::PowerLineFrequency power_line_frequency(
      const video_capture::VideoCaptureSettings& input) {
    return input.power_line_frequency;
  }

  static bool Read(video_capture::mojom::VideoCaptureSettingsDataView data,
                   video_capture::VideoCaptureSettings* out);
};
}

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_VIDEO_CAPTURE_SETTINGS_TRAITS_H_
