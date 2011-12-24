// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FrameConsumerProxy is used to allow a FrameConsumer on the UI thread to be
// invoked by a Decoder on the decoder thread.  The Detach() method is used by
// the proxy's owner before tearing down the FrameConsumer, to prevent any
// further invokations reaching it.

#ifndef REMOTING_CLIENT_FRAME_CONSUMER_PROXY_H_
#define REMOTING_CLIENT_FRAME_CONSUMER_PROXY_H_

#include "base/memory/ref_counted.h"
#include "remoting/client/frame_consumer.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace remoting {

class FrameConsumerProxy
    : public base::RefCountedThreadSafe<FrameConsumerProxy>,
      public FrameConsumer {
 public:
  // Constructs a proxy for |frame_consumer| which will trampoline invocations
  // to |frame_consumer_message_loop|.
  FrameConsumerProxy(FrameConsumer* frame_consumer,
                     base::MessageLoopProxy* frame_consumer_message_loop);
  virtual ~FrameConsumerProxy();

  // FrameConsumer implementation.
  virtual void AllocateFrame(media::VideoFrame::Format format,
                             const SkISize& size,
                             scoped_refptr<media::VideoFrame>* frame_out,
                             const base::Closure& done) OVERRIDE;
  virtual void ReleaseFrame(media::VideoFrame* frame) OVERRIDE;
  virtual void OnPartialFrameOutput(media::VideoFrame* frame,
                                    RectVector* rects,
                                    const base::Closure& done) OVERRIDE;

  // Detaches from |frame_consumer_|, ensuring no further calls reach it.
  // This must only be called from |frame_consumer_message_loop_|.
  void Detach();

 private:
  FrameConsumer* frame_consumer_;

  scoped_refptr<base::MessageLoopProxy> frame_consumer_message_loop_;

  DISALLOW_COPY_AND_ASSIGN(FrameConsumerProxy);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_FRAME_CONSUMER_PROXY_H_
