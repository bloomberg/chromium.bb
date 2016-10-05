// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/interfaces/video_capture_format_traits.h"

#include "ui/gfx/geometry/mojo/geometry.mojom.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

// static
bool StructTraits<video_capture::mojom::VideoCaptureFormatDataView,
                  media::VideoCaptureFormat>::
    Read(video_capture::mojom::VideoCaptureFormatDataView data,
         media::VideoCaptureFormat* out) {
  if (!data.ReadFrameSize(&out->frame_size))
    return false;
  out->frame_rate = data.frame_rate();

  // Since there are static_asserts in place in
  // media/mojo/common/media_type_converters.cc to guarantee equality of the
  // underlying representations, we can simply static_cast to convert.
  // TODO(mcasas): Use EnumTraits for VideoPixelFormat, https://crbug.com/651897
  out->pixel_format =
      static_cast<media::VideoPixelFormat>(data.pixel_format());
  if (!data.ReadPixelStorage(&out->pixel_storage))
    return false;
  return true;
}

// static
video_capture::mojom::VideoPixelStorage
EnumTraits<video_capture::mojom::VideoPixelStorage, media::VideoPixelStorage>::
    ToMojom(media::VideoPixelStorage video_pixel_storage) {
  switch (video_pixel_storage) {
    case media::PIXEL_STORAGE_CPU:
      return video_capture::mojom::VideoPixelStorage::CPU;
    case media::PIXEL_STORAGE_GPUMEMORYBUFFER:
      return video_capture::mojom::VideoPixelStorage::GPUMEMORYBUFFER;
  }
  NOTREACHED();
  return video_capture::mojom::VideoPixelStorage::CPU;
}

// static
bool EnumTraits<video_capture::mojom::VideoPixelStorage,
                media::VideoPixelStorage>::
    FromMojom(video_capture::mojom::VideoPixelStorage input,
              media::VideoPixelStorage* out) {
  switch (input) {
    case video_capture::mojom::VideoPixelStorage::CPU:
      *out = media::PIXEL_STORAGE_CPU;
      return true;
    case video_capture::mojom::VideoPixelStorage::GPUMEMORYBUFFER:
      *out = media::PIXEL_STORAGE_GPUMEMORYBUFFER;
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace mojo
