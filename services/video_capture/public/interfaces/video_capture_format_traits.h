// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_VIDEO_CAPTURE_FORMAT_TYPEMAP_H_
#define SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_VIDEO_CAPTURE_FORMAT_TYPEMAP_H_

#include "media/capture/video_capture_types.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "services/video_capture/public/interfaces/video_capture_format.mojom.h"

namespace mojo {

template <>
struct StructTraits<video_capture::mojom::VideoCaptureFormatDataView,
                    media::VideoCaptureFormat> {
  static const gfx::Size& frame_size(const media::VideoCaptureFormat& format) {
    return format.frame_size;
  }

  static float frame_rate(const media::VideoCaptureFormat& format) {
    return format.frame_rate;
  }

  static media::VideoPixelFormat pixel_format(
      const media::VideoCaptureFormat& format) {
    return format.pixel_format;
  }

  static video_capture::mojom::VideoPixelStorage pixel_storage(
      const media::VideoCaptureFormat& format) {
    return static_cast<video_capture::mojom::VideoPixelStorage>(
        format.pixel_storage);
}

  static bool Read(video_capture::mojom::VideoCaptureFormatDataView data,
                   media::VideoCaptureFormat* out);
};

template <>
struct EnumTraits<video_capture::mojom::VideoPixelStorage,
                  media::VideoPixelStorage> {
  static video_capture::mojom::VideoPixelStorage ToMojom(
      media::VideoPixelStorage video_pixel_storage);

  static bool FromMojom(video_capture::mojom::VideoPixelStorage input,
                        media::VideoPixelStorage* out);
};
}

#endif  // SERVICES_VIDEO_CAPTURE_PUBLIC_INTERFACES_VIDEO_CAPTURE_FORMAT_TYPEMAP_H_
