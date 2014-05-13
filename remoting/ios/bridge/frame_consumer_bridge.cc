// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/ios/bridge/frame_consumer_bridge.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "remoting/base/util.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

FrameConsumerBridge::FrameConsumerBridge(OnFrameCallback callback)
    : callback_(callback), frame_producer_(NULL) {}

FrameConsumerBridge::~FrameConsumerBridge() {
  // The producer should now return any pending buffers. At this point, however,
  // the buffers are returned via tasks which may not be scheduled before the
  // producer, so we free all the buffers once the producer's queue is empty.
  // And the scheduled tasks will die quietly.
  if (frame_producer_) {
    base::WaitableEvent done_event(true, false);
    frame_producer_->RequestReturnBuffers(base::Bind(
        &base::WaitableEvent::Signal, base::Unretained(&done_event)));
    done_event.Wait();
  }
}

void FrameConsumerBridge::Initialize(FrameProducer* producer) {
  DCHECK(!frame_producer_);
  frame_producer_ = producer;
  DCHECK(frame_producer_);
}

void FrameConsumerBridge::ApplyBuffer(const webrtc::DesktopSize& view_size,
                                      const webrtc::DesktopRect& clip_area,
                                      webrtc::DesktopFrame* buffer,
                                      const webrtc::DesktopRegion& region,
                                      const webrtc::DesktopRegion& shape) {
  DCHECK(frame_producer_);
  if (!view_size_.equals(view_size)) {
    // Drop the frame, since the data belongs to the previous generation,
    // before SetSourceSize() called SetOutputSizeAndClip().
    ReturnBuffer(buffer);
    return;
  }

  // This call completes synchronously.
  callback_.Run(view_size, buffer, region);

  // Recycle |buffer| by returning it to |frame_producer_| as the next buffer
  frame_producer_->DrawBuffer(buffer);
}

void FrameConsumerBridge::ReturnBuffer(webrtc::DesktopFrame* buffer) {
  DCHECK(frame_producer_);
  ScopedVector<webrtc::DesktopFrame>::iterator it =
      std::find(buffers_.begin(), buffers_.end(), buffer);

  DCHECK(it != buffers_.end());
  buffers_.erase(it);
}

void FrameConsumerBridge::SetSourceSize(const webrtc::DesktopSize& source_size,
                                        const webrtc::DesktopVector& dpi) {
  DCHECK(frame_producer_);
  view_size_ = source_size;
  webrtc::DesktopRect clip_area = webrtc::DesktopRect::MakeSize(view_size_);
  frame_producer_->SetOutputSizeAndClip(view_size_, clip_area);

  // Now that the size is well known, ask the producer to start drawing
  DrawWithNewBuffer();
}

FrameConsumerBridge::PixelFormat FrameConsumerBridge::GetPixelFormat() {
  return FORMAT_RGBA;
}

void FrameConsumerBridge::DrawWithNewBuffer() {
  DCHECK(frame_producer_);
  webrtc::DesktopFrame* buffer = new webrtc::BasicDesktopFrame(view_size_);
  buffers_.push_back(buffer);
  frame_producer_->DrawBuffer(buffer);
}

}  // namespace remoting
