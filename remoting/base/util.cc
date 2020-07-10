// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/util.h"

#include <math.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

enum { kBytesPerPixelRGB32 = 4 };

static int CalculateRGBOffset(int x, int y, int stride) {
  return stride * y + kBytesPerPixelRGB32 * x;
}

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

void CopyRGB32Rect(const uint8_t* source_buffer,
                   int source_stride,
                   const webrtc::DesktopRect& source_buffer_rect,
                   uint8_t* dest_buffer,
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
