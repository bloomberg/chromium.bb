// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_H_
#define REMOTING_HOST_CAPTURER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/lock.h"
#include "base/task.h"
#include "gfx/rect.h"
#include "remoting/base/protocol/chromotocol.pb.h"

namespace remoting {

typedef std::vector<gfx::Rect> RectVector;

// A class to perform the task of capturing the image of a window.
// The capture action is asynchronous to allow maximum throughput.
//
// Implementation has to ensure the following gurantees:
// 1. Double buffering
//    Since data can be read while another capture action is
//    happening.
class Capturer {
 public:

  struct DataPlanes {
    static const int kPlaneCount = 3;
    uint8* data[kPlaneCount];
    int strides[kPlaneCount];

    DataPlanes() {
      for (int i = 0; i < kPlaneCount; ++i) {
        data[i] = NULL;
        strides[i] = 0;
      }
    }
  };

  // Stores the data and information of a capture to pass off to the
  // encoding thread.
  class CaptureData : public base::RefCountedThreadSafe<CaptureData> {
   public:
    CaptureData(const DataPlanes &data_planes,
                int width,
                int height,
                PixelFormat format) :
            data_planes_(data_planes), dirty_rects_(),
            width_(width), height_(height), pixel_format_(format) { }

    // Get the data_planes data of the last capture.
    const DataPlanes& data_planes() const { return data_planes_; }

    // Get the list of updated rectangles in the last capture. The result is
    // written into |rects|.
    const RectVector& dirty_rects() const { return dirty_rects_; }

    // Get the width of the image captured.
    int width() const { return width_; }

    // Get the height of the image captured.
    int height() const { return height_; }

    // Get the pixel format of the image captured.
    PixelFormat pixel_format() const { return pixel_format_; }

    // Mutating methods.
    RectVector& mutable_dirty_rects() { return dirty_rects_; }

   private:
    const DataPlanes data_planes_;
    RectVector dirty_rects_;
    int width_;
    int height_;
    PixelFormat pixel_format_;

    friend class base::RefCountedThreadSafe<CaptureData>;
    ~CaptureData() {}
  };

  // CaptureCompletedCallback is called when the capturer has completed.
  typedef Callback1<scoped_refptr<CaptureData> >::Type CaptureCompletedCallback;

  Capturer();
  virtual ~Capturer();


  // Capture the updated regions since last capture. If the last
  // capture doesn't exist, the full window is captured.
  //
  // When complete |callback| is called.
  //
  // It is OK to call this method while another thread is reading
  // data of the last capture.
  // There can be at most one concurrent read going on when this
  // method is called.
  virtual void CaptureInvalidRects(CaptureCompletedCallback* callback);

  // Capture the specified screen rects and call |callback| when complete.
  // Dirty or invalid regions are ignored and only the given |rects| areas are
  // captured.
  //
  // It is OK to call this method while another thread is reading
  // data of the last capture.
  // There can be at most one concurrent read going on when this
  // method is called.
  virtual void CaptureRects(const RectVector& rects,
                            CaptureCompletedCallback* callback) = 0;

  // Invalidate the specified screen rects.
  virtual void InvalidateRects(const RectVector& inval_rects);

  // Invalidate the entire screen.
  virtual void InvalidateFullScreen();

  // Called when the screen configuration is changed.
  virtual void ScreenConfigurationChanged() = 0;

  // Return the width of the screen.
  virtual int width() const { return width_; }

  // Return the height of the screen
  virtual int height() const { return height_; }

  // Return the pixel format of the screen
  virtual PixelFormat pixel_format() const { return pixel_format_; }

 protected:
  // Finish/cleanup capture task.
  // This should be called at the end of each of the CaptureXxx() routines.
  // This routine should (at least):
  // (1) Call the |callback| routine.
  // (2) Select the next screen buffer.
  //     Note that capturers are required to be double-buffered so that we can
  //     read from one which capturing into another.
  virtual void FinishCapture(scoped_refptr<CaptureData> data,
                             CaptureCompletedCallback* callback);

  // Number of screen buffers.
  static const int kNumBuffers = 2;

  // Capture screen dimensions.
  int width_;
  int height_;

  // Format of pixels returned in buffer.
  PixelFormat pixel_format_;
  int bytes_per_row_;

  // The current buffer with valid data for reading.
  int current_buffer_;

 private:

  // Rects that have been manually invalidated (through InvalidateRect).
  // These will be returned as dirty_rects in the capture data during the next
  // capture.
  RectVector inval_rects_;

  // A lock protecting |inval_rects_| across threads.
  Lock inval_rects_lock_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_H_
