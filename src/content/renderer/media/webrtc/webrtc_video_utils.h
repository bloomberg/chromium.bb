// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_UTILS_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_UTILS_H_

#include "media/base/video_color_space.h"
#include "media/base/video_transformation.h"
#include "third_party/webrtc/api/video/color_space.h"
#include "third_party/webrtc/api/video/video_rotation.h"

namespace content {

// This file has helper methods for conversion between chromium types and
// webrtc/api/video types.

media::VideoRotation WebRtcToMediaVideoRotation(webrtc::VideoRotation rotation);

media::VideoColorSpace WebRtcToMediaVideoColorSpace(
    const webrtc::ColorSpace& color_space);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_WEBRTC_VIDEO_UTILS_H_
