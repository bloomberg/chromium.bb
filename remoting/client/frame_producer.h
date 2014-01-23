// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_FRAME_PRODUCER_H_
#define REMOTING_CLIENT_FRAME_PRODUCER_H_

#include "base/callback_forward.h"

namespace webrtc {
class DesktopFrame;
class DesktopRect;
class DesktopRegion;
class DesktopSize;
}  // namespace webrtc

namespace remoting {

class FrameProducer {
 public:
  FrameProducer() {}

  // Adds an image buffer to the pool of pending buffers for subsequent drawing.
  // Once drawing is completed the buffer will be returned to the consumer via
  // the FrameConsumer::ApplyBuffer() call. Alternatively an empty buffer could
  // be returned via the FrameConsumer::ReturnBuffer() call.
  //
  // The passed buffer must be large enough to hold the whole clipping area.
  virtual void DrawBuffer(webrtc::DesktopFrame* buffer) = 0;

  // Requests repainting of the specified |region| of the frame as soon as
  // possible. |region| is specified in output coordinates relative to
  // the beginning of the frame.
  virtual void InvalidateRegion(const webrtc::DesktopRegion& region) = 0;

  // Requests returing of all pending buffers to the consumer via
  // FrameConsumer::ReturnBuffer() calls.
  virtual void RequestReturnBuffers(const base::Closure& done) = 0;

  // Notifies the producer of changes to the output view size or clipping area.
  // Implementations must cope with empty |view_size| or |clip_area|.
  virtual void SetOutputSizeAndClip(const webrtc::DesktopSize& view_size,
                                    const webrtc::DesktopRect& clip_area) = 0;

 protected:
  virtual ~FrameProducer() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameProducer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_FRAME_PRODUCER_H_
