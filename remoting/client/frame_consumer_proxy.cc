// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/frame_consumer_proxy.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "ppapi/cpp/image_data.h"

namespace remoting {

FrameConsumerProxy::FrameConsumerProxy(
    scoped_refptr<base::MessageLoopProxy> frame_consumer_message_loop)
    : frame_consumer_message_loop_(frame_consumer_message_loop) {
}

void FrameConsumerProxy::ApplyBuffer(const SkISize& view_size,
                                     const SkIRect& clip_area,
                                     pp::ImageData* buffer,
                                     const SkRegion& region) {
  if (!frame_consumer_message_loop_->BelongsToCurrentThread()) {
    frame_consumer_message_loop_->PostTask(FROM_HERE, base::Bind(
        &FrameConsumerProxy::ApplyBuffer, this,
        view_size, clip_area, buffer, region));
    return;
  }

  if (frame_consumer_)
    frame_consumer_->ApplyBuffer(view_size, clip_area, buffer, region);
}

void FrameConsumerProxy::ReturnBuffer(pp::ImageData* buffer) {
  if (!frame_consumer_message_loop_->BelongsToCurrentThread()) {
    frame_consumer_message_loop_->PostTask(FROM_HERE, base::Bind(
        &FrameConsumerProxy::ReturnBuffer, this, buffer));
    return;
  }

  if (frame_consumer_)
    frame_consumer_->ReturnBuffer(buffer);
}

void FrameConsumerProxy::SetSourceSize(const SkISize& source_size) {
  if (!frame_consumer_message_loop_->BelongsToCurrentThread()) {
    frame_consumer_message_loop_->PostTask(FROM_HERE, base::Bind(
        &FrameConsumerProxy::SetSourceSize, this, source_size));
    return;
  }

  if (frame_consumer_)
    frame_consumer_->SetSourceSize(source_size);
}

void FrameConsumerProxy::Attach(
    const base::WeakPtr<FrameConsumer>& frame_consumer) {
  DCHECK(frame_consumer_message_loop_->BelongsToCurrentThread());
  DCHECK(frame_consumer_ == NULL);
  frame_consumer_ = frame_consumer;
}

FrameConsumerProxy::~FrameConsumerProxy() {
  DCHECK(frame_consumer_message_loop_->BelongsToCurrentThread());
}

}  // namespace remoting
