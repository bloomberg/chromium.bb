// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_H_
#define REMOTING_HOST_CAPTURER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/task.h"
#include "gfx/rect.h"
#include "remoting/base/protocol/chromotocol.pb.h"

namespace remoting {

typedef std::vector<gfx::Rect> DirtyRects;

// A class to perform the task of capturing the image of a window.
// The capture action is asynchronous to allow maximum throughput.
//
// Implementation has to ensure the following gurantees:
// 1. Double buffering
//    Since data can be read while another capture action is
//    happening.
class Capturer {
 public:
  Capturer();
  virtual ~Capturer();

  // Capture the full screen. When the action is completed |done_task|
  // is called.
  //
  // It is OK to call this methods while another thread is reading
  // data of the last capture.
  // There can be at most one concurrent read going on when this
  // methods is called.
  virtual void CaptureFullScreen(Task* done_task) = 0;

  // Capture the updated regions since last capture. If the last
  // capture doesn't exist, the full window is captured.
  //
  // When complete |done_task| is called.
  //
  // It is OK to call this method while another thread is reading
  // data of the last capture.
  // There can be at most one concurrent read going on when this
  // methods is called.
  virtual void CaptureDirtyRects(Task* done_task) = 0;

  // Capture the specified screen rect and call |done_task| when complete.
  // Dirty or invalid regions are ignored and only the given |rect| area is
  // captured.
  //
  // It is OK to call this method while another thread is reading
  // data of the last capture.
  // There can be at most one concurrent read going on when this
  // methods is called.
  virtual void CaptureRect(const gfx::Rect& rect, Task* done_task) = 0;

  // Get the image data of the last capture. The pointers to data is
  // written to |planes|. |planes| should be an array of 3 elements.
  virtual void GetData(const uint8* planes[]) const = 0;

  // Get the image data stride of the last capture. This size of strides
  // is written to |strides|. |strides| should be array of 3 elements.
  virtual void GetDataStride(int strides[]) const = 0;

  // Get the list of updated rectangles in the last capture. The result is
  // written into |rects|.
  virtual void GetDirtyRects(DirtyRects* rects) const;

  // Get the width of the image captured.
  virtual int GetWidth() const;

  // Get the height of the image captured.
  virtual int GetHeight() const;

  // Get the pixel format of the image captured.
  virtual PixelFormat GetPixelFormat() const;

  // Invalidate the specified screen rect.
  virtual void InvalidateRect(gfx::Rect dirty);

 protected:
  // Finish/cleanup capture task.
  // This should be called at the end of each of the CaptureXxx() routines.
  // This routine should (at least):
  // (1) Call the |done_task| routine.
  // (2) Select the next screen buffer.
  //     Note that capturers are required to be double-buffered so that we can
  //     read from one which capturing into another.
  virtual void FinishCapture(Task* done_task);

  // Number of screen buffers.
  static const int kNumBuffers = 2;

  // Capture screen dimensions.
  int width_;
  int height_;

  // Format of pixels returned in buffer.
  PixelFormat pixel_format_;

  // Information about screen.
  int bytes_per_pixel_;
  int bytes_per_row_;

  // The current buffer with valid data for reading.
  int current_buffer_;

  // List of dirty rects.
  // These are the rects that we send to the client to update.
  DirtyRects dirty_rects_;

  // Rects that have been manually invalidated (through InvalidateRect).
  // These will be merged into |dirty_rects_| during the next capture.
  DirtyRects inval_rects_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_H_
