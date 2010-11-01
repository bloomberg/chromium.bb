// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/util.h"

#include "base/logging.h"

namespace remoting {

int GetBytesPerPixel(PixelFormat format) {
  // Note: The order is important here for performance. This is sorted from the
  // most common to the less common (PixelFormatAscii is mostly used
  // just for testing).
  switch (format) {
    case PixelFormatRgb24:  return 3;
    case PixelFormatRgb565: return 2;
    case PixelFormatRgb32:  return 4;
    case PixelFormatAscii:  return 1;
    default:
      NOTREACHED() << "Pixel format not supported";
      return 0;
  }
}

}  // namespace remoting
