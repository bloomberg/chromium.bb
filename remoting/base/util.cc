// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"
#include "remoting/base/util.h"

using media::VideoFrame;

namespace remoting {

std::string GetTimestampString() {
  base::Time t = base::Time::NowFromSystemTime();
  base::Time::Exploded tex;
  t.LocalExplode(&tex);
  return StringPrintf("%02d%02d/%02d%02d%02d:",
                      tex.month, tex.day_of_month,
                      tex.hour, tex.minute, tex.second);
}

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
                               const SkIRect& rect,
                               int y_stride,
                               int uv_stride,
                               int rgb_stride) {
  int rgb_offset = CalculateRGBOffset(rect.fLeft, rect.fTop, rgb_stride);
  int y_offset = CalculateYOffset(rect.fLeft, rect.fTop, y_stride);
  int uv_offset = CalculateUVOffset(rect.fLeft, rect.fTop, uv_stride);

  media::ConvertYUVToRGB32(y_plane + y_offset,
                           u_plane + uv_offset,
                           v_plane + uv_offset,
                           rgb_plane + rgb_offset,
                           rect.width(),
                           rect.height(),
                           y_stride,
                           uv_stride,
                           rgb_stride,
                           media::YV12);
}

void ScaleYUVToRGB32WithRect(const uint8* y_plane,
                             const uint8* u_plane,
                             const uint8* v_plane,
                             uint8* rgb_plane,
                             const SkIRect& source_rect,
                             const SkIRect& dest_rect,
                             int y_stride,
                             int uv_stride,
                             int rgb_stride) {
  int rgb_offset = CalculateRGBOffset(dest_rect.fLeft,
                                      dest_rect.fTop,
                                      rgb_stride);
  int y_offset = CalculateYOffset(source_rect.fLeft,
                                  source_rect.fTop,
                                  y_stride);
  int uv_offset = CalculateUVOffset(source_rect.fLeft,
                                    source_rect.fTop,
                                    uv_stride);

  media::ScaleYUVToRGB32(y_plane + y_offset,
                         u_plane + uv_offset,
                         v_plane + uv_offset,
                         rgb_plane + rgb_offset,
                         source_rect.width(),
                         source_rect.height(),
                         dest_rect.width(),
                         dest_rect.height(),
                         y_stride,
                         uv_stride,
                         rgb_stride,
                         media::YV12,
                         media::ROTATE_0,
                         media::FILTER_NONE);
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

int RoundToTwosMultiple(int x) {
  return x & (~1);
}

SkIRect AlignRect(const SkIRect& rect) {
  int x = RoundToTwosMultiple(rect.fLeft);
  int y = RoundToTwosMultiple(rect.fTop);
  int right = RoundToTwosMultiple(rect.fRight + 1);
  int bottom = RoundToTwosMultiple(rect.fBottom + 1);
  return SkIRect::MakeXYWH(x, y, right - x, bottom - y);
}

SkIRect ScaleRect(const SkIRect& rect,
                  double horizontal_ratio,
                  double vertical_ratio) {
  int x = rect.fLeft * horizontal_ratio;
  int y = rect.fTop * vertical_ratio;
  int w = rect.fRight * horizontal_ratio - x;
  int h = rect.fBottom * vertical_ratio - y;

  return SkIRect::MakeXYWH(x, y, w, h);
}

void CopyRect(const uint8* src_plane,
              int src_plane_stride,
              uint8* dest_plane,
              int dest_plane_stride,
              int bytes_per_pixel,
              const SkIRect& rect) {
 // Get the address of the starting point.
  const int src_y_offset = src_plane_stride * rect.fTop;
  const int dest_y_offset = dest_plane_stride * rect.fTop;
  const int x_offset = bytes_per_pixel * rect.fLeft;
  src_plane += src_y_offset + x_offset;
  dest_plane += dest_y_offset + x_offset;

  // Copy pixels in the rectangle line by line.
  const int bytes_per_line = bytes_per_pixel * rect.width();
  const int height = rect.height();
  for (int i = 0 ; i < height; ++i) {
    memcpy(dest_plane, src_plane, bytes_per_line);
    src_plane += src_plane_stride;
    dest_plane += dest_plane_stride;
  }
}

}  // namespace remoting
