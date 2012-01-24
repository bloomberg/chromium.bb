// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/frame_consumer_proxy.h"

#include "base/bind.h"
#include "base/message_loop.h"

namespace remoting {

FrameConsumerProxy::FrameConsumerProxy(
    FrameConsumer* frame_consumer,
    base::MessageLoopProxy* frame_consumer_message_loop)
    : frame_consumer_(frame_consumer),
      frame_consumer_message_loop_(frame_consumer_message_loop) {
}

FrameConsumerProxy::~FrameConsumerProxy() {
}

void FrameConsumerProxy::AllocateFrame(
    media::VideoFrame::Format format,
    const SkISize& size,
    scoped_refptr<media::VideoFrame>* frame_out,
    const base::Closure& done) {
  if (!frame_consumer_message_loop_->BelongsToCurrentThread()) {
    frame_consumer_message_loop_->PostTask(FROM_HERE, base::Bind(
        &FrameConsumerProxy::AllocateFrame, this, format, size, frame_out,
        done));
    return;
  }

  if (frame_consumer_) {
    frame_consumer_->AllocateFrame(format, size, frame_out, done);
  }
}

void FrameConsumerProxy::ReleaseFrame(media::VideoFrame* frame) {
  if (!frame_consumer_message_loop_->BelongsToCurrentThread()) {
    frame_consumer_message_loop_->PostTask(FROM_HERE, base::Bind(
        &FrameConsumerProxy::ReleaseFrame, this, make_scoped_refptr(frame)));
    return;
  }

  if (frame_consumer_)
    frame_consumer_->ReleaseFrame(frame);
}

void FrameConsumerProxy::OnPartialFrameOutput(media::VideoFrame* frame,
                                              RectVector* rects,
                                              const base::Closure& done) {
  if (!frame_consumer_message_loop_->BelongsToCurrentThread()) {
    frame_consumer_message_loop_->PostTask(FROM_HERE, base::Bind(
        &FrameConsumerProxy::OnPartialFrameOutput, this,
        make_scoped_refptr(frame), rects, done));
    return;
  }

  if (frame_consumer_)
    frame_consumer_->OnPartialFrameOutput(frame, rects, done);
}

void FrameConsumerProxy::Detach() {
  DCHECK(frame_consumer_message_loop_->BelongsToCurrentThread());
  frame_consumer_ = NULL;
}

}  // namespace remoting
