// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/frame_consumer_proxy.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

FrameConsumerProxy::FrameConsumerProxy(
    const base::WeakPtr<FrameConsumer>& frame_consumer)
    : frame_consumer_(frame_consumer),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {
  pixel_format_ = frame_consumer_->GetPixelFormat();
}

static void DoApplyBuffer(base::WeakPtr<FrameConsumer> frame_consumer,
                          const webrtc::DesktopSize& view_size,
                          const webrtc::DesktopRect& clip_area,
                          webrtc::DesktopFrame* buffer,
                          const webrtc::DesktopRegion& region,
                          scoped_ptr<webrtc::DesktopRegion> shape) {
  if (!frame_consumer)
    return;

  frame_consumer->ApplyBuffer(view_size, clip_area, buffer, region,
                              shape.get());
}

void FrameConsumerProxy::ApplyBuffer(const webrtc::DesktopSize& view_size,
                                     const webrtc::DesktopRect& clip_area,
                                     webrtc::DesktopFrame* buffer,
                                     const webrtc::DesktopRegion& region,
                                     const webrtc::DesktopRegion* shape) {
  scoped_ptr<webrtc::DesktopRegion> shape_ptr;
  if (shape)
    shape_ptr = make_scoped_ptr(new webrtc::DesktopRegion(*shape));
  task_runner_->PostTask(
      FROM_HERE,
      base::Bind(DoApplyBuffer, frame_consumer_, view_size, clip_area, buffer,
                 region, base::Passed(&shape_ptr)));
}

void FrameConsumerProxy::ReturnBuffer(webrtc::DesktopFrame* buffer) {
  task_runner_->PostTask(FROM_HERE, base::Bind(&FrameConsumer::ReturnBuffer,
                                               frame_consumer_, buffer));
}

void FrameConsumerProxy::SetSourceSize(
    const webrtc::DesktopSize& source_size,
    const webrtc::DesktopVector& source_dpi) {
  task_runner_->PostTask(
      FROM_HERE, base::Bind(&FrameConsumer::SetSourceSize, frame_consumer_,
                            source_size, source_dpi));
}

FrameConsumer::PixelFormat FrameConsumerProxy::GetPixelFormat() {
  return pixel_format_;
}

FrameConsumerProxy::~FrameConsumerProxy() {
}

}  // namespace remoting
