// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_FRAME_CONSUMER_H_
#define REMOTING_CLIENT_FRAME_CONSUMER_H_

#include "base/basictypes.h"

namespace webrtc {
class DesktopFrame;
class DesktopRect;
class DesktopRegion;
class DesktopSize;
class DesktopVector;
}  // namespace webrtc

namespace remoting {

class FrameConsumer {
 public:

  // List of supported pixel formats needed by various platforms.
  enum PixelFormat {
    FORMAT_BGRA,  // Used by the Pepper plugin.
    FORMAT_RGBA,  // Used for Android's Bitmap class.
  };

  // Paints the contents of |buffer| into the area of the view identified
  // by |clip_area|. |view_size| specifies the full-frame dimensions to which
  // the |buffer|/|clip_area| portion was scaled. Implementations may be
  // optimized to only paint pixels within the intersection of |region| and
  // |clip_area|. If |shape| is non-NULL then it specifies the complete shape
  // of the frame, otherwise the frame is un-shaped.
  //
  // N.B. |clip_area|, |region| and |shape| should be provided in output view
  // coordinates.
  virtual void ApplyBuffer(const webrtc::DesktopSize& view_size,
                           const webrtc::DesktopRect& clip_area,
                           webrtc::DesktopFrame* buffer,
                           const webrtc::DesktopRegion& region,
                           const webrtc::DesktopRegion* shape) = 0;

  // Accepts a buffer that couldn't be used for drawing for any reason (shutdown
  // is in progress, the view area has changed, etc.). The accepted buffer can
  // be freed or reused for another drawing operation.
  virtual void ReturnBuffer(webrtc::DesktopFrame* buffer) = 0;

  // Set the dimension of the entire host screen.
  virtual void SetSourceSize(const webrtc::DesktopSize& source_size,
                             const webrtc::DesktopVector& dpi) = 0;

  // Returns the preferred pixel encoding for the platform.
  virtual PixelFormat GetPixelFormat() = 0;

 protected:
  FrameConsumer() {}
  virtual ~FrameConsumer() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameConsumer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_FRAME_CONSUMER_H_
