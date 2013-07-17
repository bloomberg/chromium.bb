// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_FRAME_CONSUMER_H_
#define REMOTING_CLIENT_JNI_JNI_FRAME_CONSUMER_H_

#include "remoting/client/frame_consumer.h"

#include "base/compiler_specific.h"

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {
class ChromotingJni;
class FrameProducer;

// FrameConsumer implementation that draws onto a JNI direct byte buffer.
class JniFrameConsumer : public FrameConsumer {
 public:
  JniFrameConsumer();
  virtual ~JniFrameConsumer();

  // This must be called once before the producer's source size is set.
  void set_frame_producer(FrameProducer* producer);

  // FrameConsumer implementation.
  virtual void ApplyBuffer(const SkISize& view_size,
                           const SkIRect& clip_area,
                           webrtc::DesktopFrame* buffer,
                           const SkRegion& region) OVERRIDE;
  virtual void ReturnBuffer(webrtc::DesktopFrame* buffer) OVERRIDE;
  virtual void SetSourceSize(const SkISize& source_size,
                             const SkIPoint& dpi) OVERRIDE;

 private:
  // Variables are to be used from the display thread.

  // Whether to allocate/provide the producer with a buffer when able. This
  // goes to false during destruction so that we don't leak memory.
  bool provide_buffer_;

  FrameProducer* frame_producer_;
  SkISize view_size_;
  SkIRect clip_area_;

  // If |provide_buffer_|, allocates a new buffer of |view_size_|, informs
  // Java about it, and tells the producer to draw onto it. Otherwise, no-op.
  void AllocateBuffer();

  DISALLOW_COPY_AND_ASSIGN(JniFrameConsumer);
};

}  // namespace remoting

#endif
