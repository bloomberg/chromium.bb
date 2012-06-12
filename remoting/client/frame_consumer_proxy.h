// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FrameConsumerProxy is used to allow a FrameConsumer on the UI thread to be
// invoked by a Decoder on the decoder thread.  The Detach() method is used by
// the proxy's owner before tearing down the FrameConsumer, to prevent any
// further invokations reaching it.

#ifndef REMOTING_CLIENT_FRAME_CONSUMER_PROXY_H_
#define REMOTING_CLIENT_FRAME_CONSUMER_PROXY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/frame_consumer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace remoting {

class FrameConsumerProxy
    : public base::RefCountedThreadSafe<FrameConsumerProxy>,
      public FrameConsumer {
 public:
  // Constructs a proxy for |frame_consumer| which will trampoline invocations
  // to |frame_consumer_message_loop|.
  FrameConsumerProxy(scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // FrameConsumer implementation.
  virtual void ApplyBuffer(const SkISize& view_size,
                           const SkIRect& clip_area,
                           pp::ImageData* buffer,
                           const SkRegion& region) OVERRIDE;
  virtual void ReturnBuffer(pp::ImageData* buffer) OVERRIDE;
  virtual void SetSourceSize(const SkISize& source_size) OVERRIDE;

  // Attaches to |frame_consumer_|.
  // This must only be called from |frame_consumer_message_loop_|.
  void Attach(const base::WeakPtr<FrameConsumer>& frame_consumer);

 private:
  friend class base::RefCountedThreadSafe<FrameConsumerProxy>;
  virtual ~FrameConsumerProxy();

  base::WeakPtr<FrameConsumer> frame_consumer_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(FrameConsumerProxy);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_FRAME_CONSUMER_PROXY_H_
