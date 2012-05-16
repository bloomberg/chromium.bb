// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Don't include this file in any .h files because it pulls in some X headers.

#ifndef REMOTING_HOST_X_SERVER_PIXEL_BUFFER_H_
#define REMOTING_HOST_X_SERVER_PIXEL_BUFFER_H_

#include "base/basictypes.h"
#include "third_party/skia/include/core/SkRect.h"

#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

namespace remoting {

// A class to allow the X server's pixel buffer to be accessed as efficiently
// as possible.
class XServerPixelBuffer {
 public:
  XServerPixelBuffer();
  ~XServerPixelBuffer();

  void Release();
  void Init(Display* display);

  // If shared memory is being used without pixmaps, synchronize this pixel
  // buffer with the root window contents (otherwise, this is a no-op).
  // This is to avoid doing a full-screen capture for each individual
  // rectangle in the capture list, when it only needs to be done once at the
  // beginning.
  void Synchronize();

  // Capture the specified rectangle and return a pointer to its top-left pixel
  // or NULL if capture fails. The returned pointer remains valid until the next
  // call to CaptureRect.
  // In the case where the full-screen data is captured by Synchronize(), this
  // simply returns the pointer without doing any more work.
  uint8* CaptureRect(const SkIRect& rect);

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
