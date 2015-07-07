// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "remoting/host/chromeos/skia_bitmap_desktop_frame.h"

namespace remoting {

// static
SkiaBitmapDesktopFrame* SkiaBitmapDesktopFrame::Create(
    scoped_ptr<SkBitmap> bitmap) {

  webrtc::DesktopSize size(bitmap->width(), bitmap->height());
  DCHECK_EQ(kBGRA_8888_SkColorType, bitmap->info().colorType())
      << "DesktopFrame objects always hold RGBA data.";

  bitmap->lockPixels();
  uint8_t* bitmap_data = reinterpret_cast<uint8_t*>(bitmap->getPixels());
  bitmap->unlockPixels();

  const size_t row_bytes = bitmap->rowBytes();
  SkiaBitmapDesktopFrame* result = new SkiaBitmapDesktopFrame(
      size, row_bytes, bitmap_data, bitmap.Pass());

  return result;
}

SkiaBitmapDesktopFrame::SkiaBitmapDesktopFrame(webrtc::DesktopSize size,
                                               int stride,
                                               uint8_t* data,
                                               scoped_ptr<SkBitmap> bitmap)
    : DesktopFrame(size, stride, data, nullptr), bitmap_(bitmap.Pass()) {
}

SkiaBitmapDesktopFrame::~SkiaBitmapDesktopFrame() {
}

}  // namespace remoting
