// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

}  // namespace remoting
