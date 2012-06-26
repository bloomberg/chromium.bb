// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_FRAME_CONSUMER_H_
#define REMOTING_CLIENT_FRAME_CONSUMER_H_

#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkSize.h"

namespace pp {
class ImageData;
} // namespace pp

namespace remoting {

class FrameConsumer {
 public:
  // Accepts a buffer to be painted to the screen. The buffer's dimensions and
  // relative position within the frame are specified by |clip_area|. Only
  // pixels falling within |region| and the current clipping area are painted.
  // The function assumes that the passed buffer was scaled to fit a window
  // having |view_size| dimensions.
  //
  // N.B. Both |clip_area| and |region| are in output coordinates relative to
  // the frame.
  virtual void ApplyBuffer(const SkISize& view_size,
                           const SkIRect& clip_area,
                           pp::ImageData* buffer,
                           const SkRegion& region) = 0;

  // Accepts a buffer that couldn't be used for drawing for any reason (shutdown
  // is in progress, the view area has changed, etc.). The accepted buffer can
  // be freed or reused for another drawing operation.
  virtual void ReturnBuffer(pp::ImageData* buffer) = 0;

  // Set the dimension of the entire host screen.
  virtual void SetSourceSize(const SkISize& source_size) = 0;

 protected:
  FrameConsumer() {}
  virtual ~FrameConsumer() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FrameConsumer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_FRAME_CONSUMER_H_
