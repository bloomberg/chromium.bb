// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/frame_consumer_proxy.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

FrameConsumerProxy::FrameConsumerProxy(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::WeakPtr<FrameConsumer>& frame_consumer)
    : frame_consumer_(frame_consumer),
      task_runner_(task_runner) {
  pixel_format_ = frame_consumer_->GetPixelFormat();
}

void FrameConsumerProxy::ApplyBuffer(const webrtc::DesktopSize& view_size,
                                     const webrtc::DesktopRect& clip_area,
                                     webrtc::DesktopFrame* buffer,
                                     const webrtc::DesktopRegion& region,
                                     const webrtc::DesktopRegion& shape) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        &FrameConsumerProxy::ApplyBuffer, this,
        view_size, clip_area, buffer, region, shape));
    return;
  }

  if (frame_consumer_.get())
    frame_consumer_->ApplyBuffer(view_size, clip_area, buffer, region, shape);
}

void FrameConsumerProxy::ReturnBuffer(webrtc::DesktopFrame* buffer) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        &FrameConsumerProxy::ReturnBuffer, this, buffer));
    return;
  }

  if (frame_consumer_.get())
    frame_consumer_->ReturnBuffer(buffer);
}

void FrameConsumerProxy::SetSourceSize(
    const webrtc::DesktopSize& source_size,
    const webrtc::DesktopVector& source_dpi) {
  if (!task_runner_->BelongsToCurrentThread()) {
    task_runner_->PostTask(FROM_HERE, base::Bind(
        &FrameConsumerProxy::SetSourceSize, this, source_size, source_dpi));
    return;
  }

  if (frame_consumer_.get())
    frame_consumer_->SetSourceSize(source_size, source_dpi);
}

FrameConsumer::PixelFormat FrameConsumerProxy::GetPixelFormat() {
  return pixel_format_;
}

FrameConsumerProxy::~FrameConsumerProxy() {
}

}  // namespace remoting
