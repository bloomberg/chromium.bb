// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_capture/public/interfaces/video_capture_settings_traits.h"

#include "media/capture/mojo/video_capture_types_typemap_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

// static
bool StructTraits<video_capture::mojom::VideoCaptureFormatDataView,
                  video_capture::VideoCaptureFormat>::
    Read(video_capture::mojom::VideoCaptureFormatDataView data,
         video_capture::VideoCaptureFormat* out) {
  if (!data.ReadFrameSize(&out->frame_size))
    return false;
  out->frame_rate = data.frame_rate();
  return true;
}

// static
bool StructTraits<video_capture::mojom::VideoCaptureSettingsDataView,
                  video_capture::VideoCaptureSettings>::
    Read(video_capture::mojom::VideoCaptureSettingsDataView data,
         video_capture::VideoCaptureSettings* out) {
  if (!data.ReadFormat(&out->format))
    return false;
  if (!data.ReadResolutionChangePolicy(&out->resolution_change_policy))
    return false;
  if (!data.ReadPowerLineFrequency(&out->power_line_frequency))
    return false;
  return true;
}


}  // namespace mojo
