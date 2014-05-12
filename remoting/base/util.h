// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_UTIL_H_
#define REMOTING_BASE_UTIL_H_

#include <string>

#include "media/base/video_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

// Return a string that contains the current date formatted as 'MMDD/HHMMSS:'.
std::string GetTimestampString();

// Calculate the offset of a specific pixel in an RGB32 buffer.
int CalculateRGBOffset(int x, int y, int stride);

// Calculate the offset of a specific pixel in a YV12/YUV420 buffer. Note that
// the X and Y coordinates must both be even owing to the YV12 buffer layout.
int CalculateYOffset(int x, int y, int stride);
int CalculateUVOffset(int x, int y, int stride);

// Convert and scale YUV to RGB32 on a specific rectangle. The source and
// destination buffers are assumed to contain only |source_buffer_rect| and
// |dest_buffer_rect| areas correspondingly. The scaling factor is determined
// as ratio between |dest_size| and |source_size|. The target rectangle
// |dect_rect| is specified in the destination coordinates.
//
// |source_buffer_rect| and |dest_buffer_rect| must fall entirely within
// the source and destination dimensions, respectively. |dest_rect| must be
// completely contained within the source and destinations buffers boundaries
// including the case when scaling is requested.
//
// N.B. The top left corner coordinates of YUV buffer should have even X and Y
// coordinates.
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
    const webrtc::DesktopRect& dest_rect);

int RoundToTwosMultiple(int x);

// Align the sides of the rectangle to multiples of 2 (expanding outwards).
webrtc::DesktopRect AlignRect(const webrtc::DesktopRect& rect);

// Scales the supplied rectangle from |in_size| coordinates to |out_size|.
// If the result has non-integer coordinates then the smallest integer-
// coordinate rectangle that wholly encloses it is returned.
webrtc::DesktopRect ScaleRect(const webrtc::DesktopRect& rect,
                              const webrtc::DesktopSize& in_size,
                              const webrtc::DesktopSize& out_size);

// Copy content of a rectangle in a RGB32 image.
void CopyRGB32Rect(const uint8* source_buffer,
                   int source_stride,
                   const webrtc::DesktopRect& source_buffer_rect,
                   uint8* dest_buffer,
                   int dest_stride,
                   const webrtc::DesktopRect& dest_buffer_rect,
                   const webrtc::DesktopRect& dest_rect);

// Replaces every occurrence of "\n" in a string by "\r\n".
std::string ReplaceLfByCrLf(const std::string& in);

// Replaces every occurrence of "\r\n" in a string by "\n".
std::string ReplaceCrLfByLf(const std::string& in);

// Checks if the given string is a valid UTF-8 string.
bool StringIsUtf8(const char* data, size_t length);

bool DoesRectContain(const webrtc::DesktopRect& a,
                     const webrtc::DesktopRect& b);

}  // namespace remoting

#endif  // REMOTING_BASE_UTIL_H_
