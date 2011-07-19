// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"
#include "remoting/base/util.h"

#include "base/logging.h"

using media::VideoFrame;

namespace remoting {

int GetBytesPerPixel(VideoFrame::Format format) {
  // Note: The order is important here for performance. This is sorted from the
  // most common to the less common (PIXEL_FORMAT_ASCII is mostly used
  // just for testing).
  switch (format) {
    case VideoFrame::RGB24:  return 3;
    case VideoFrame::RGB565: return 2;
    case VideoFrame::RGB32:  return 4;
    case VideoFrame::ASCII:  return 1;
    default:
      NOTREACHED() << "Pixel format not supported";
      return 0;
  }
}

// Helper methods to calculate plane offset given the coordinates.
static int CalculateRGBOffset(int x, int y, int stride) {
  return stride * y + GetBytesPerPixel(media::VideoFrame::RGB32) * x;
}

static int CalculateYOffset(int x, int y, int stride) {
  return stride * y + x;
}

static int CalculateUVOffset(int x, int y, int stride) {
  return stride * y / 2 + x / 2;
}

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
                               int rgb_stride) {
  int rgb_offset = CalculateRGBOffset(x, y, rgb_stride);
  int y_offset = CalculateYOffset(x, y, y_stride);
  int uv_offset = CalculateUVOffset(x, y, uv_stride);;

  media::ConvertYUVToRGB32(y_plane + y_offset,
                           u_plane + uv_offset,
                           v_plane + uv_offset,
                           rgb_plane + rgb_offset,
                           width,
                           height,
                           y_stride,
                           uv_stride,
                           rgb_stride,
                           media::YV12);
}

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
                               int uv_stride) {
  int rgb_offset = CalculateRGBOffset(x, y, rgb_stride);
  int y_offset = CalculateYOffset(x, y, y_stride);
  int uv_offset = CalculateUVOffset(x, y, uv_stride);;

  media::ConvertRGB32ToYUV(rgb_plane + rgb_offset,
                           y_plane + y_offset,
                           u_plane + uv_offset,
                           v_plane + uv_offset,
                           width,
                           height,
                           rgb_stride,
                           y_stride,
                           uv_stride);
}

}  // namespace remoting
