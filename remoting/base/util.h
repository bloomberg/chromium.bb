// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_UTIL_H_
#define REMOTING_BASE_UTIL_H_

#include "media/base/video_frame.h"

namespace remoting {

// TODO(sergeyu): Move these methods to media.
int GetBytesPerPixel(media::VideoFrame::Format format);

// Convert YUV to RGB32 on a specific rectangle.
void ConvertYUVToRGB32WithRect(const uint8* y_plane,
                               const uint8* u_plane,
                               const uint8* v_plane,
                               uint8* rgb_plane,
                               int x,
                               int y,
                               int width,
                               int height,
                               int y_stride,
                               int uv_stride,
                               int rgb_stride);

void ConvertRGB32ToYUVWithRect(const uint8* rgb_plane,
                               uint8* y_plane,
                               uint8* u_plane,
                               uint8* v_plane,
                               int x,
                               int y,
                               int width,
                               int height,
                               int rgb_stride,
                               int y_stride,
                               int uv_stride);

}  // namespace remoting

#endif  // REMOTING_BASE_UTIL_H_
