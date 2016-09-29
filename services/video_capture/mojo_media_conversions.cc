// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/mojo_media_conversions.h"

namespace video_capture {

media::VideoCaptureFormat ConvertFromMojoToMedia(
    mojom::VideoCaptureFormatPtr format) {
  media::VideoCaptureFormat result;
  result.pixel_format = ConvertFromMojoToMedia(format->pixel_format);
  result.pixel_storage = ConvertFromMojoToMedia(format->pixel_storage);
  result.frame_size.SetSize(format->frame_size.width(),
                            format->frame_size.height());
  result.frame_rate = format->frame_rate;
  return result;
}

media::VideoPixelFormat ConvertFromMojoToMedia(
    media::mojom::VideoFormat format) {
  // Since there are static_asserts in place in
  // media/mojo/common/media_type_converters.cc to guarantee equality of the
  // underlying representations, we can simply static_cast to convert.
  return static_cast<media::VideoPixelFormat>(format);
}

media::VideoPixelStorage ConvertFromMojoToMedia(
    mojom::VideoPixelStorage storage) {
  switch (storage) {
    case mojom::VideoPixelStorage::CPU:
      return media::PIXEL_STORAGE_CPU;
    case mojom::VideoPixelStorage::GPUMEMORYBUFFER:
      return media::PIXEL_STORAGE_GPUMEMORYBUFFER;
  }
  NOTREACHED();
  return media::PIXEL_STORAGE_CPU;
}

media::ResolutionChangePolicy ConvertFromMojoToMedia(
    mojom::ResolutionChangePolicy policy) {
  switch (policy) {
    case mojom::ResolutionChangePolicy::FIXED_RESOLUTION:
      return media::RESOLUTION_POLICY_FIXED_RESOLUTION;
    case mojom::ResolutionChangePolicy::FIXED_ASPECT_RATIO:
      return media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO;
    case mojom::ResolutionChangePolicy::ANY_WITHIN_LIMIT:
      return media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT;
  }
  NOTREACHED();
  return media::RESOLUTION_POLICY_FIXED_RESOLUTION;
}

media::PowerLineFrequency ConvertFromMojoToMedia(
    mojom::PowerLineFrequency frequency) {
  switch (frequency) {
    case mojom::PowerLineFrequency::DEFAULT:
      return media::PowerLineFrequency::FREQUENCY_DEFAULT;
    case mojom::PowerLineFrequency::HZ_50:
      return media::PowerLineFrequency::FREQUENCY_50HZ;
    case mojom::PowerLineFrequency::HZ_60:
      return media::PowerLineFrequency::FREQUENCY_60HZ;
  }
  NOTREACHED();
  return media::PowerLineFrequency::FREQUENCY_DEFAULT;
}

}  // namespace video_capture
