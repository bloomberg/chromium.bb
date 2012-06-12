// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/frame_consumer_proxy.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "ppapi/cpp/image_data.h"

namespace remoting {

FrameConsumerProxy::FrameConsumerProxy(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner) {
}

void FrameConsumerProxy::ApplyBuffer(const SkISize& view_size,
                                     const SkIRect& clip_area,
                                     pp::ImageData* buffer,
                                     const SkRegion& region) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        &FrameConsumerProxy::ApplyBuffer, this,
        view_size, clip_area, buffer, region));
    return;
  }

  if (frame_consumer_)
    frame_consumer_->ApplyBuffer(view_size, clip_area, buffer, region);
}

void FrameConsumerProxy::ReturnBuffer(pp::ImageData* buffer) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        &FrameConsumerProxy::ReturnBuffer, this, buffer));
    return;
  }

  if (frame_consumer_)
    frame_consumer_->ReturnBuffer(buffer);
}

void FrameConsumerProxy::SetSourceSize(const SkISize& source_size) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        &FrameConsumerProxy::SetSourceSize, this, source_size));
    return;
  }

  if (frame_consumer_)
    frame_consumer_->SetSourceSize(source_size);
}

void FrameConsumerProxy::Attach(
    const base::WeakPtr<FrameConsumer>& frame_consumer) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(frame_consumer_ == NULL);
  frame_consumer_ = frame_consumer;
}

FrameConsumerProxy::~FrameConsumerProxy() {
}

}  // namespace remoting
