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

  virtual scoped_ptr<webrtc::DesktopFrame> AllocateFrame(
      const webrtc::DesktopSize& size) = 0;

  virtual void DrawFrame(scoped_ptr<webrtc::DesktopFrame> frame,
                         const base::Closure& done) = 0;

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
