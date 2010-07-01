// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_GDI_H_
#define REMOTING_HOST_CAPTURER_GDI_H_

#include <windows.h>
typedef HBITMAP BitmapRef;
#include "base/scoped_ptr.h"
#include "remoting/host/capturer.h"

namespace remoting {

// CapturerGdi captures 32bit RGB using GDI.
//
// CapturerGdi is doubled buffered as required by Capturer. See
// remoting/host/capturer.h.
class CapturerGdi : public Capturer {
 public:
  CapturerGdi();
  virtual ~CapturerGdi();

  virtual void CaptureRects(const RectVector& rects,
                            CaptureCompletedCallback* callback);
  virtual void ScreenConfigurationChanged();

 private:
  void ReleaseBuffers();
  // Generates an image in the current buffer.
  void CaptureImage();

  // Gdi specific information about screen.
  HDC desktop_dc_;
  HDC memory_dc_;
  HBITMAP target_bitmap_[kNumBuffers];

  // We have two buffers for the screen images as required by Capturer.
  void* buffers_[kNumBuffers];

  DISALLOW_COPY_AND_ASSIGN(CapturerGdi);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_GDI_H_
