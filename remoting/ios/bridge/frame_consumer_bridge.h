// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_IOS_BRIDGE_FRAME_CONSUMER_BRIDGE_H_
#define REMOTING_IOS_BRIDGE_FRAME_CONSUMER_BRIDGE_H_

#include <list>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/frame_consumer.h"
#include "remoting/client/frame_producer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

class FrameConsumerBridge : public base::SupportsWeakPtr<FrameConsumerBridge>,
                            public FrameConsumer {
 public:
  typedef base::Callback<void(const webrtc::DesktopSize& view_size,
                              webrtc::DesktopFrame* buffer,
                              const webrtc::DesktopRegion& region)>
      OnFrameCallback;

  // A callback is provided to return frame updates asynchronously.
  explicit FrameConsumerBridge(OnFrameCallback callback);

  virtual ~FrameConsumerBridge();
  // This must be called before any other functional use.
  void Initialize(FrameProducer* producer);

  // FrameConsumer implementation.
  virtual void ApplyBuffer(const webrtc::DesktopSize& view_size,
                           const webrtc::DesktopRect& clip_area,
                           webrtc::DesktopFrame* buffer,
                           const webrtc::DesktopRegion& region,
                           const webrtc::DesktopRegion& shape) OVERRIDE;
  virtual void ReturnBuffer(webrtc::DesktopFrame* buffer) OVERRIDE;
  virtual void SetSourceSize(const webrtc::DesktopSize& source_size,
                             const webrtc::DesktopVector& dpi) OVERRIDE;
  virtual PixelFormat GetPixelFormat() OVERRIDE;

 private:
  // Allocates a new buffer of |view_size_|, and tells the producer to draw onto
  // it.  This can be called as soon as the producer is known, but is not
  // required until ready to receive frames.
  void DrawWithNewBuffer();

  OnFrameCallback callback_;

  FrameProducer* frame_producer_;
  webrtc::DesktopSize view_size_;

  // List of allocated image buffers.
  ScopedVector<webrtc::DesktopFrame> buffers_;

  DISALLOW_COPY_AND_ASSIGN(FrameConsumerBridge);
};

}  // namespace remoting

#endif  // REMOTING_IOS_BRIDGE_FRAME_CONSUMER_BRIDGE_H_
