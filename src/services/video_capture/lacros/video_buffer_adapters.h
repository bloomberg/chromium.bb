// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_LACROS_VIDEO_BUFFER_ADAPTERS_H_
#define SERVICES_VIDEO_CAPTURE_LACROS_VIDEO_BUFFER_ADAPTERS_H_

#include "chromeos/crosapi/mojom/video_capture.mojom.h"
#include "services/video_capture/public/mojom/video_frame_handler.mojom.h"

namespace video_capture {

media::mojom::VideoBufferHandlePtr ConvertToMediaVideoBuffer(
    crosapi::mojom::VideoBufferHandlePtr buffer_handle);
media::mojom::VideoFrameInfoPtr ConvertToMediaVideoFrameInfo(
    crosapi::mojom::VideoFrameInfoPtr buffer_info);
}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_LACROS_VIDEO_BUFFER_ADAPTERS_H_
