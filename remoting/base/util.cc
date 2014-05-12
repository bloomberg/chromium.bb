// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/util.h"

#include <math.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "media/base/yuv_convert.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

using media::VideoFrame;

namespace remoting {

enum { kBytesPerPixelRGB32 = 4 };

// Do not write LOG messages in this routine since it is called from within
// our LOG message handler. Bad things will happen.
std::string GetTimestampString() {
  base::Time t = base::Time::NowFromSystemTime();
  base::Time::Exploded tex;
  t.LocalExplode(&tex);
  return base::StringPrintf("%02d%02d/%02d%02d%02d:",
                            tex.month, tex.day_of_month,
                            tex.hour, tex.minute, tex.second);
}

int CalculateRGBOffset(int x, int y, int stride) {
  return stride * y + kBytesPerPixelRGB32 * x;
}

int CalculateYOffset(int x, int y, int stride) {
  DCHECK(((x & 1) == 0) && ((y & 1) == 0));
  return stride * y + x;
}

int CalculateUVOffset(int x, int y, int stride) {
  DCHECK(((x & 1) == 0) && ((y & 1) == 0));
  return stride * y / 2 + x / 2;
}

void ConvertAndScaleYUVToRGB32Rect(
    const uint8* source_yplane,
    const uint8* source_uplane,
    const uint8* source_vplane,
    int source_ystride,
    int source_uvstride,
    const webrtc::DesktopSize& source_size,
    const webrtc::DesktopRect& source_buffer_rect,
    uint8* dest_buffer,
    int dest_stride,
    const webrtc::DesktopSize& dest_size,
    const webrtc::DesktopRect& dest_buffer_rect,
    const webrtc::DesktopRect& dest_rect) {
  // N.B. It is caller's responsibility to check if strides are large enough. We
  // cannot do it here anyway.
  DCHECK(DoesRectContain(webrtc::DesktopRect::MakeSize(source_size),
                         source_buffer_rect));
  DCHECK(DoesRectContain(webrtc::DesktopRect::MakeSize(dest_size),
                         dest_buffer_rect));
  DCHECK(DoesRectContain(dest_buffer_rect, dest_rect));
  DCHECK(DoesRectContain(ScaleRect(source_buffer_rect, source_size, dest_size),
                         dest_rect));

  // If the source and/or destination buffers don't start at (0, 0)
  // offset the pointers to pretend we have complete buffers.
  int y_offset = - CalculateYOffset(source_buffer_rect.left(),
                                    source_buffer_rect.top(),
                                    source_ystride);
  int uv_offset = - CalculateUVOffset(source_buffer_rect.left(),
                                      source_buffer_rect.top(),
                                      source_uvstride);
  int rgb_offset = - CalculateRGBOffset(dest_buffer_rect.left(),
                                        dest_buffer_rect.top(),
                                        dest_stride);

  // See if scaling is needed.
  if (source_size.equals(dest_size)) {
    // Calculate the inner rectangle that can be copied by the optimized
    // libyuv::I420ToARGB().
    webrtc::DesktopRect inner_rect =
        webrtc::DesktopRect::MakeLTRB(RoundToTwosMultiple(dest_rect.left() + 1),
                                      RoundToTwosMultiple(dest_rect.top() + 1),
                                      dest_rect.right(), dest_rect.bottom());

    // Offset pointers to point to the top left corner of the inner rectangle.
    y_offset += CalculateYOffset(inner_rect.left(), inner_rect.top(),
                                 source_ystride);
    uv_offset += CalculateUVOffset(inner_rect.left(), inner_rect.top(),
                                   source_uvstride);
    rgb_offset += CalculateRGBOffset(inner_rect.left(), inner_rect.top(),
                                     dest_stride);

    libyuv::I420ToARGB(source_yplane + y_offset, source_ystride,
                       source_uplane + uv_offset, source_uvstride,
                       source_vplane + uv_offset, source_uvstride,
                       dest_buffer + rgb_offset, dest_stride,
                       inner_rect.width(), inner_rect.height());

    // Now see if some pixels weren't copied due to alignment.
    if (!dest_rect.equals(inner_rect)) {
      webrtc::DesktopRect outer_rect =
          webrtc::DesktopRect::MakeLTRB(RoundToTwosMultiple(dest_rect.left()),
                                        RoundToTwosMultiple(dest_rect.top()),
                                        dest_rect.right(), dest_rect.bottom());

      webrtc::DesktopVector offset(outer_rect.left() - inner_rect.left(),
                                   outer_rect.top() - inner_rect.top());

      // Offset the pointers to point to the top left corner of the outer
      // rectangle.
      y_offset += CalculateYOffset(offset.x(), offset.y(), source_ystride);
      uv_offset += CalculateUVOffset(offset.x(), offset.y(), source_uvstride);
      rgb_offset += CalculateRGBOffset(offset.x(), offset.y(), dest_stride);

      // Draw unaligned edges.
      webrtc::DesktopRegion edges(dest_rect);
      edges.Subtract(inner_rect);
      for (webrtc::DesktopRegion::Iterator i(edges); !i.IsAtEnd();
           i.Advance()) {
        webrtc::DesktopRect rect = i.rect();
        rect.Translate(-outer_rect.left(), -outer_rect.top());
        media::ScaleYUVToRGB32WithRect(source_yplane + y_offset,
                                       source_uplane + uv_offset,
                                       source_vplane + uv_offset,
                                       dest_buffer + rgb_offset,
                                       source_size.width(),
                                       source_size.height(),
                                       dest_size.width(),
                                       dest_size.height(),
                                       rect.left(),
                                       rect.top(),
                                       rect.right(),
                                       rect.bottom(),
                                       source_ystride,
                                       source_uvstride,
                                       dest_stride);
      }
    }
  } else {
    media::ScaleYUVToRGB32WithRect(source_yplane + y_offset,
                                   source_uplane + uv_offset,
                                   source_vplane + uv_offset,
                                   dest_buffer + rgb_offset,
                                   source_size.width(),
                                   source_size.height(),
                                   dest_size.width(),
                                   dest_size.height(),
                                   dest_rect.left(),
                                   dest_rect.top(),
                                   dest_rect.right(),
                                   dest_rect.bottom(),
                                   source_ystride,
                                   source_uvstride,
                                   dest_stride);
  }
}

int RoundToTwosMultiple(int x) {
  return x & (~1);
}

webrtc::DesktopRect AlignRect(const webrtc::DesktopRect& rect) {
  int x = RoundToTwosMultiple(rect.left());
  int y = RoundToTwosMultiple(rect.top());
  int right = RoundToTwosMultiple(rect.right() + 1);
  int bottom = RoundToTwosMultiple(rect.bottom() + 1);
  return webrtc::DesktopRect::MakeLTRB(x, y, right, bottom);
}

webrtc::DesktopRect ScaleRect(const webrtc::DesktopRect& rect,
                              const webrtc::DesktopSize& in_size,
                              const webrtc::DesktopSize& out_size) {
  int left = (rect.left() * out_size.width()) / in_size.width();
  int top = (rect.top() * out_size.height()) / in_size.height();
  int right = (rect.right() * out_size.width() + in_size.width() - 1) /
      in_size.width();
  int bottom = (rect.bottom() * out_size.height() + in_size.height() - 1) /
      in_size.height();
  return webrtc::DesktopRect::MakeLTRB(left, top, right, bottom);
}

void CopyRGB32Rect(const uint8* source_buffer,
                   int source_stride,
                   const webrtc::DesktopRect& source_buffer_rect,
                   uint8* dest_buffer,
                   int dest_stride,
                   const webrtc::DesktopRect& dest_buffer_rect,
                   const webrtc::DesktopRect& dest_rect) {
  DCHECK(DoesRectContain(dest_buffer_rect, dest_rect));
  DCHECK(DoesRectContain(source_buffer_rect, dest_rect));

  // Get the address of the starting point.
  source_buffer += CalculateRGBOffset(
      dest_rect.left() - source_buffer_rect.left(),
      dest_rect.top() - source_buffer_rect.top(),
      source_stride);
  dest_buffer += CalculateRGBOffset(
      dest_rect.left() - dest_buffer_rect.left(),
      dest_rect.top() - dest_buffer_rect.top(),
      source_stride);

  // Copy pixels in the rectangle line by line.
  const int bytes_per_line = kBytesPerPixelRGB32 * dest_rect.width();
  for (int i = 0 ; i < dest_rect.height(); ++i) {
    memcpy(dest_buffer, source_buffer, bytes_per_line);
    source_buffer += source_stride;
    dest_buffer += dest_stride;
  }
}

std::string ReplaceLfByCrLf(const std::string& in) {
  std::string out;
  out.resize(2 * in.size());
  char* out_p_begin = &out[0];
  char* out_p = out_p_begin;
  const char* in_p_begin = &in[0];
  const char* in_p_end = &in[in.size()];
  for (const char* in_p = in_p_begin; in_p < in_p_end; ++in_p) {
    char c = *in_p;
    if (c == '\n') {
      *out_p++ = '\r';
    }
    *out_p++ = c;
  }
  out.resize(out_p - out_p_begin);
  return out;
}

std::string ReplaceCrLfByLf(const std::string& in) {
  std::string out;
  out.resize(in.size());
  char* out_p_begin = &out[0];
  char* out_p = out_p_begin;
  const char* in_p_begin = &in[0];
  const char* in_p_end = &in[in.size()];
  for (const char* in_p = in_p_begin; in_p < in_p_end; ++in_p) {
    char c = *in_p;
    if ((c == '\r') && (in_p + 1 < in_p_end) && (*(in_p + 1) == '\n')) {
      *out_p++ = '\n';
      ++in_p;
    } else {
      *out_p++ = c;
    }
  }
  out.resize(out_p - out_p_begin);
  return out;
}

bool StringIsUtf8(const char* data, size_t length) {
  const char* ptr = data;
  const char* ptr_end = data + length;
  while (ptr != ptr_end) {
    if ((*ptr & 0x80) == 0) {
      // Single-byte symbol.
      ++ptr;
    } else if ((*ptr & 0xc0) == 0x80 || (*ptr & 0xfe) == 0xfe) {
      // Invalid first byte.
      return false;
    } else {
      // First byte of a multi-byte symbol. The bits from 2 to 6 are the count
      // of continuation bytes (up to 5 of them).
      for (char first = *ptr << 1; first & 0x80; first <<= 1) {
        ++ptr;

        // Missing continuation byte.
        if (ptr == ptr_end)
          return false;

        // Invalid continuation byte.
        if ((*ptr & 0xc0) != 0x80)
          return false;
      }

      ++ptr;
    }
  }

  return true;
}

bool DoesRectContain(const webrtc::DesktopRect& a,
                     const webrtc::DesktopRect& b) {
  webrtc::DesktopRect intersection(a);
  intersection.IntersectWith(b);
  return intersection.equals(b);
}

}  // namespace remoting
