// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_H_
#define REMOTING_HOST_CAPTURER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "remoting/base/capture_data.h"
#include "remoting/base/types.h"

class MessageLoop;

namespace remoting {

// A class to perform the task of capturing the image of a window.
// The capture action is asynchronous to allow maximum throughput.
//
// The full capture process is as follows:
//
// (1) InvalidateRects
//     This is an optional step where regions of the screen are marked as
//     invalid. Some platforms (Windows, for now) won't use this and will
//     instead calculate the diff-regions later (in step (2). Other
//     platforms (Mac) will use this to mark all the changed regions of the
//     screen. Some limited rect-merging (e.g., to eliminate exact
//     duplicates) may be done here.
//
// (2) CaptureInvalidRects
//     This is where the bits for the invalid rects are packaged up and sent
//     to the encoder.
//     A screen capture is performed if needed. For example, Windows requires
//     a capture to calculate the diff from the previous screen, whereas the
//     Mac version does not.
//
// Implementation has to ensure the following guarantees:
// 1. Double buffering
//    Since data can be read while another capture action is happening.
class Capturer {
 public:
  // CaptureCompletedCallback is called when the capturer has completed.
  typedef Callback1<scoped_refptr<CaptureData> >::Type CaptureCompletedCallback;

  virtual ~Capturer();

  // Create platform-specific cpaturer.
  static Capturer* Create(MessageLoop* message_loop);

  // Called when the screen configuration is changed.
  virtual void ScreenConfigurationChanged() = 0;

  // Return the width of the screen.
  virtual int width() const;

  // Return the height of the screen.
  virtual int height() const;

  // Return the pixel format of the screen.
  virtual media::VideoFrame::Format pixel_format() const;

  // Clear out the list of invalid rects.
  void ClearInvalidRects();

  // Invalidate the specified screen rects.
  void InvalidateRects(const InvalidRects& inval_rects);

  // Invalidate the entire screen.
  virtual void InvalidateFullScreen();

  // Capture the screen data associated with each of the accumulated
  // rects in |inval_rects|.
  // This routine will first call CalculateInvalidRects to update the
  // list of |inval_rects|.
  // When the capture is complete, |callback| is called.
  //
  // If |inval_rects_| is empty, then this does nothing except
  // call the |callback| routine.
  //
  // It is OK to call this method while another thread is reading
  // data of the last capture.
  // There can be at most one concurrent read going on when this
  // method is called.
  virtual void CaptureInvalidRects(CaptureCompletedCallback* callback);

 protected:
  explicit Capturer(MessageLoop* message_loop);

  // Update the list of |invalid_rects| to prepare for capturing the
  // screen data.
  // Depending on the platform implementation, this routine might:
  // (a) Analyze screen and calculate the list of rects that have changed
  //     since the last capture.
  // (b) Merge already-acculumated rects into a more optimal list (for
  //     example, by combining or removing rects).
  virtual void CalculateInvalidRects() = 0;

  // Capture the specified screen rects and call |callback| when complete.
  // Dirty or invalid regions are ignored and only the given |rects| areas are
  // captured.
  // This routine is used internally by CaptureInvalidRects().
  virtual void CaptureRects(const InvalidRects& rects,
                            CaptureCompletedCallback* callback) = 0;

  // Finish/cleanup capture task.
  // This should be called by CaptureRects() when it finishes.
  // This routine should (at least):
  // (1) Call the |callback| routine.
  // (2) Select the next screen buffer.
  //     Note that capturers are required to be double-buffered so that we can
  //     read from one which capturing into another.
  void FinishCapture(scoped_refptr<CaptureData> data,
                     CaptureCompletedCallback* callback);

  // Called by subclasses' CalculateInvalidRects() method to check if
  // InvalidateFullScreen() was called by user.
  bool IsCaptureFullScreen();

  // Number of screen buffers.
  static const int kNumBuffers = 2;

  // Capture screen dimensions.
  int width_;
  int height_;

  // Format of pixels returned in buffer.
  media::VideoFrame::Format pixel_format_;
  int bytes_per_row_;

  // The current buffer with valid data for reading.
  int current_buffer_;

  // Message loop that operations should run on.
  MessageLoop* message_loop_;

 private:
  // Rects that have been manually invalidated (through InvalidateRect).
  // These will be returned as dirty_rects in the capture data during the next
  // capture.
  InvalidRects inval_rects_;

  // A lock protecting |inval_rects_| across threads.
  base::Lock inval_rects_lock_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_H_
