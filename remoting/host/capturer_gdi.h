// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_GDI_H_
#define REMOTING_HOST_CAPTURER_GDI_H_

#include <windows.h>
typedef HBITMAP BitmapRef;
#include "base/memory/scoped_ptr.h"
#include "remoting/host/capturer.h"
#include "remoting/host/capturer_helper.h"

namespace remoting {

class Differ;

// CapturerGdi captures 32bit RGB using GDI.
//
// CapturerGdi is double-buffered as required by Capturer. See
// remoting/host/capturer.h.
class CapturerGdi : public Capturer {
 public:
  CapturerGdi();
  virtual ~CapturerGdi();

  // Capturer interface.
  virtual void ScreenConfigurationChanged();
  virtual media::VideoFrame::Format pixel_format() const;
  virtual void ClearInvalidRects();
  virtual void InvalidateRects(const InvalidRects& inval_rects);
  virtual void InvalidateScreen(const gfx::Size& size);
  virtual void InvalidateFullScreen();
  virtual void CaptureInvalidRects(CaptureCompletedCallback* callback);
  virtual const gfx::Size& size_most_recent() const;

 private:
  struct VideoFrameBuffer {
    VideoFrameBuffer(void* data, const gfx::Size& size, int bytes_per_pixel,
                     int bytes_per_row)
      : data(data), size(size), bytes_per_pixel(bytes_per_pixel),
        bytes_per_row(bytes_per_row) {
    }
    VideoFrameBuffer() {
      data = 0;
      size = gfx::Size(0, 0);
      bytes_per_pixel = 0;
      bytes_per_row = 0;
    }
    void* data;
    gfx::Size size;
    int bytes_per_pixel;
    int bytes_per_row;
  };

  // Make sure that the current buffer has the same size as the screen.
  void UpdateBufferCapture(const gfx::Size& size);

  // Allocate memory for a buffer of a given size, freeing any memory previously
  // allocated for that buffer.
  void ReallocateBuffer(int buffer_index, const gfx::Size& size);

  void CalculateInvalidRects();
  void CaptureRects(const InvalidRects& rects,
                    CaptureCompletedCallback* callback);

  void ReleaseBuffers();
  // Generates an image in the current buffer.
  void CaptureImage();

  // Gets the current screen size and calls ScreenConfigurationChanged
  // if the screen size has changed.
  void MaybeChangeScreenConfiguration();

  // Gets the screen size.
  gfx::Size GetScreenSize();

  // A thread-safe list of invalid rectangles, and the size of the most
  // recently captured screen.
  CapturerHelper helper;

  // There are two buffers for the screen images, as required by Capturer.
  static const int kNumBuffers = 2;
  VideoFrameBuffer buffers_[kNumBuffers];

  // Gdi specific information about screen.
  HDC desktop_dc_;
  HDC memory_dc_;
  HBITMAP target_bitmap_[kNumBuffers];

  // The screen size attached to the device contexts through which the screen
  // is captured.
  gfx::Size dc_size_;

  // The current buffer with valid data for reading.
  int current_buffer_;

  // Format of pixels returned in buffer.
  media::VideoFrame::Format pixel_format_;

  // Class to calculate the difference between two screen bitmaps.
  scoped_ptr<Differ> differ_;

  // True if we should force a fullscreen capture.
  bool capture_fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(CapturerGdi);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_GDI_H_
