// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_UTIL_H_
#define REMOTING_BASE_UTIL_H_

#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/gfx/rect.h"

namespace remoting {

std::string GetTimestampString();

// TODO(sergeyu): Move these methods to media.
int GetBytesPerPixel(media::VideoFrame::Format format);

// Convert YUV to RGB32 on a specific rectangle.
void ConvertYUVToRGB32WithRect(const uint8* y_plane,
                               const uint8* u_plane,
                               const uint8* v_plane,
                               uint8* rgb_plane,
                               const gfx::Rect& rect,
                               int y_stride,
                               int uv_stride,
                               int rgb_stride);

void ScaleYUVToRGB32WithRect(const uint8* y_plane,
                             const uint8* u_plane,
                             const uint8* v_plane,
                             uint8* rgb_plane,
                             const gfx::Rect& source_rect,
                             const gfx::Rect& dest_rect,
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

int RoundToTwosMultiple(int x);

// Align the sides of the rectangle to multiples of 2 (expanding outwards).
gfx::Rect AlignRect(const gfx::Rect& rect);

// Return a scaled rectangle using the horizontal and vertical scale
// factors.
gfx::Rect ScaleRect(const gfx::Rect& rect,
                    double horizontal_ratio,
                    double vertical_ratio);

// Copy pixels in the rectangle from source to destination.
void CopyRect(const uint8* src_plane,
              int src_plane_stride,
              uint8* dest_plane,
              int dest_plane_stride,
              int bytes_per_pixel,
              const SkIRect& rect);

}  // namespace remoting

#endif  // REMOTING_BASE_UTIL_H_
