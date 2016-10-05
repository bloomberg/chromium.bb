// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_MOJO_MEDIA_CONVERSIONS_H_
#define SERVICES_VIDEO_CAPTURE_MOJO_MEDIA_CONVERSIONS_H_

#include "media/capture/video/video_capture_device.h"
#include "services/video_capture/public/interfaces/video_capture_device_factory.mojom.h"
#include "services/video_capture/public/interfaces/video_capture_device_proxy.mojom.h"
#include "services/video_capture/public/interfaces/video_capture_format.mojom.h"

namespace video_capture {

// TODO(chfremer): Consider using Mojo type mapping instead of conversion
// methods. https://crbug.com/642387

media::ResolutionChangePolicy ConvertFromMojoToMedia(
    mojom::ResolutionChangePolicy policy);
media::PowerLineFrequency ConvertFromMojoToMedia(
    mojom::PowerLineFrequency frequency);

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_MOJO_MEDIA_CONVERSIONS_H_
