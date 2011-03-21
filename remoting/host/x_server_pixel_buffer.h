// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Don't include this file in any .h files because it pulls in some X headers.

#ifndef REMOTING_HOST_X_SERVER_PIXEL_BUFFER_H_
#define REMOTING_HOST_X_SERVER_PIXEL_BUFFER_H_

#include "base/scoped_ptr.h"
#include "base/basictypes.h"
#include "ui/gfx/rect.h"

#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

namespace remoting {

// A class to allow the X server's pixel buffer to be accessed as efficiently
// as possible.
class XServerPixelBuffer {
 public:
  XServerPixelBuffer();
  ~XServerPixelBuffer();

  void Init(Display* display);

  // Capture the specified rectangle and return a pointer to its top-left pixel
  // or NULL if capture fails. The returned pointer remains valid until the next
  // call to CaptureRect.
  uint8* CaptureRect(const gfx::Rect& rect);

  // Return information about the most recent capture. This is only guaranteed
  // to be valid between CaptureRect calls.
  int GetStride() const;
  int GetDepth() const;
  int GetBitsPerPixel() const;
  int GetRedMask() const;
  int GetBlueMask() const;
  int GetGreenMask() const;
  int GetRedShift() const;
  int GetBlueShift() const;
  int GetGreenShift() const;
  bool IsRgb() const;

 private:
  void InitShm(int screen);
  bool InitPixmaps(int width, int height, int depth);
  void DestroyShmSegmentInfo();

  Display* display_;
  Window root_window_;
  XImage* x_image_;
  XShmSegmentInfo* shm_segment_info_;
  Pixmap shm_pixmap_;
  GC shm_gc_;

  DISALLOW_COPY_AND_ASSIGN(XServerPixelBuffer);
};

}  // namespace remoting

#endif  // REMOTING_HOST_X_SERVER_PIXEL_BUFFER_H_
