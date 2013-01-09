// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CAPTURER_VIDEO_FRAME_CAPTURER_H_
#define REMOTING_CAPTURER_VIDEO_FRAME_CAPTURER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/shared_memory.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace remoting {

class CaptureData;
struct MouseCursorShape;
class SharedBufferFactory;

// Class used to capture video frames asynchronously.
//
// The full capture sequence is as follows:
//
// (1) Start
//     This is when pre-capture steps are executed, such as flagging the
//     display to prevent it from sleeping during a session.
//
// (2) InvalidateRegion
//     This is an optional step where regions of the screen are marked as
//     invalid. Some platforms (Windows, for now) won't use this and will
//     instead calculate the diff-regions later (in step (2). Other
//     platforms (Mac) will use this to mark all the changed regions of the
//     screen. Some limited rect-merging (e.g., to eliminate exact
//     duplicates) may be done here.
//
// (3) CaptureFrame
//     This is where the bits for the invalid rects are packaged up and sent
//     to the encoder.
//     A screen capture is performed if needed. For example, Windows requires
//     a capture to calculate the diff from the previous screen, whereas the
//     Mac version does not.
//
// (4) Stop
//     This is when post-capture steps are executed, such as releasing the
//     assertion that prevents the display from sleeping.
//
// Implementation has to ensure the following guarantees:
// 1. Double buffering
//    Since data can be read while another capture action is happening.
class VideoFrameCapturer {
 public:
  // Provides callbacks used by the capturer to pass captured video frames and
  // mouse cursor shapes to the processing pipeline.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when a video frame has been captured. |capture_data| describes
    // a captured frame.
    virtual void OnCaptureCompleted(
        scoped_refptr<CaptureData> capture_data) = 0;

    // Called when the cursor shape has changed.
    // TODO(sergeyu): Move cursor shape capturing to a separate class.
    virtual void OnCursorShapeChanged(
        scoped_ptr<MouseCursorShape> cursor_shape) = 0;
  };

  virtual ~VideoFrameCapturer() {}

  // Create platform-specific capturer.
  static scoped_ptr<VideoFrameCapturer> Create();

  // Create platform-specific capturer that uses shared memory buffers.
  static scoped_ptr<VideoFrameCapturer> CreateWithFactory(
      SharedBufferFactory* shared_buffer_factory);

#if defined(OS_LINUX)
  // Set whether the VideoFrameCapturer should try to use X DAMAGE support if it
  // is available. This needs to be called before the VideoFrameCapturer is
  // created.
  // This is used by the Virtual Me2Me host, since the XDamage extension is
  // known to work reliably in this case.

  // TODO(lambroslambrou): This currently sets a global flag, referenced during
  // VideoFrameCapturer::Create().  This is a temporary solution, until the
  // DesktopEnvironment class is refactored to allow applications to control
  // the creation of various stubs (including the VideoFrameCapturer) - see
  // http://crbug.com/104544
  static void EnableXDamage(bool enable);
#endif  // defined(OS_LINUX)

  // Called at the beginning of a capturing session. |delegate| must remain
  // valid until Stop() is called.
  virtual void Start(Delegate* delegate) = 0;

  // Called at the end of a capturing session.
  virtual void Stop() = 0;

  // Invalidates the specified region.
  virtual void InvalidateRegion(const SkRegion& invalid_region) = 0;

  // Captures the screen data associated with each of the accumulated
  // dirty region. When the capture is complete, the delegate is notified even
  // if the dirty region is empty.
  //
  // It is OK to call this method while another thread is reading
  // data of the previous capture. There can be at most one concurrent read
  // going on when this method is called.
  virtual void CaptureFrame() = 0;
};

}  // namespace remoting

#endif  // REMOTING_CAPTURER_VIDEO_FRAME_CAPTURER_H_
