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

class Differ;

// CapturerGdi captures 32bit RGB using GDI.
//
// CapturerGdi is doubled buffered as required by Capturer. See
// remoting/host/capturer.h.
class CapturerGdi : public Capturer {
 public:
  explicit CapturerGdi(MessageLoop* message_loop);
  virtual ~CapturerGdi();

  virtual void ScreenConfigurationChanged();

 private:
  virtual void CalculateInvalidRects();
  virtual void CaptureRects(const InvalidRects& rects,
                            CaptureCompletedCallback* callback);

  void ReleaseBuffers();
  // Generates an image in the current buffer.
  void CaptureImage();

  // Gdi specific information about screen.
  HDC desktop_dc_;
  HDC memory_dc_;
  HBITMAP target_bitmap_[kNumBuffers];

  // We have two buffers for the screen images as required by Capturer.
  void* buffers_[kNumBuffers];

  // Class to calculate the difference between two screen bitmaps.
  scoped_ptr<Differ> differ_;

  // True if we should force a fullscreen capture.
  bool capture_fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(CapturerGdi);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_GDI_H_
