// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CAPTURER_H_
#define REMOTING_HOST_CAPTURER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "media/base/video_frame.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace remoting {

namespace protocol {
class CursorShapeInfo;
}

class CaptureData;

// A class to perform the task of capturing the image of a window.
// The capture action is asynchronous to allow maximum throughput.
//
// The full capture process is as follows:
//
// (1) Start
//     This is when pre-capture steps are executed, such as flagging the
//     display to prevent it from sleeping during a session.
//
// (2) InvalidateRects
//     This is an optional step where regions of the screen are marked as
//     invalid. Some platforms (Windows, for now) won't use this and will
//     instead calculate the diff-regions later (in step (2). Other
//     platforms (Mac) will use this to mark all the changed regions of the
//     screen. Some limited rect-merging (e.g., to eliminate exact
//     duplicates) may be done here.
//
// (3) CaptureInvalidRects
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
class Capturer {
 public:
  // CaptureCompletedCallback is called when the capturer has completed.
  typedef base::Callback<void(scoped_refptr<CaptureData>)>
      CaptureCompletedCallback;

  // CursorShapeChangedCallback is called when the cursor shape has changed.
  typedef base::Callback<void(scoped_ptr<protocol::CursorShapeInfo>)>
      CursorShapeChangedCallback;

  virtual ~Capturer() {};

  // Create platform-specific capturer.
  static Capturer* Create();

#if defined(OS_LINUX)
  // Set whether the Capturer should try to use X DAMAGE support if it is
  // available.  This needs to be called before the Capturer is created.
  // This is used by the Virtual Me2Me host, since the XDamage extension is
  // known to work reliably in this case.

  // TODO(lambroslambrou): This currently sets a global flag, referenced during
  // Capturer::Create().  This is a temporary solution, until the
  // DesktopEnvironment class is refactored to allow applications to control
  // the creation of various stubs (including the Capturer) - see
  // http://crbug.com/104544
  static void EnableXDamage(bool enable);
#endif  // defined(OS_LINUX)

  // Called at the beginning of a capturing session.
  virtual void Start(
      const CursorShapeChangedCallback& callback) = 0;

  // Called at the end of a capturing session.
  virtual void Stop() = 0;

  // Called when the screen configuration is changed.
  virtual void ScreenConfigurationChanged() = 0;

  // Return the pixel format of the screen.
  virtual media::VideoFrame::Format pixel_format() const = 0;

  // Clear out the invalid region.
  virtual void ClearInvalidRegion() = 0;

  // Invalidate the specified region.
  virtual void InvalidateRegion(const SkRegion& invalid_region) = 0;

  // Invalidate the entire screen, of a given size.
  virtual void InvalidateScreen(const SkISize& size) = 0;

  // Invalidate the entire screen, using the size of the most recently
  // captured screen.
  virtual void InvalidateFullScreen() = 0;

  // Capture the screen data associated with each of the accumulated
  // dirty region.
  // When the capture is complete, |callback| is called even if the dirty region
  // is empty.
  //
  // It is OK to call this method while another thread is reading
  // data of the previous capture.
  // There can be at most one concurrent read going on when this
  // method is called.
  virtual void CaptureInvalidRegion(
      const CaptureCompletedCallback& callback) = 0;

  // Get the size of the most recently captured screen.
  virtual const SkISize& size_most_recent() const = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_CAPTURER_H_
