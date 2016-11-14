// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/ios/bridge/frame_consumer_bridge.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "remoting/base/util.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {

FrameConsumerBridge::FrameConsumerBridge(ClientInstance* client_instance,
                                         const OnFrameCallback callback)
    : callback_(callback), runtime_(client_instance), weak_factory_(this) {}

FrameConsumerBridge::~FrameConsumerBridge() {}

std::unique_ptr<webrtc::DesktopFrame> FrameConsumerBridge::AllocateFrame(
    const webrtc::DesktopSize& size) {
  return base::WrapUnique(new webrtc::BasicDesktopFrame(size));
}

void FrameConsumerBridge::RenderFrame(
    std::unique_ptr<webrtc::DesktopFrame> frame) {
  DCHECK(runtime_->display_task_runner()->BelongsToCurrentThread());
  // Draw the frame
  callback_.Run(frame.get());
}

void FrameConsumerBridge::DrawFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                                    const base::Closure& done) {
  DCHECK(runtime_->network_task_runner()->BelongsToCurrentThread());

  runtime_->display_task_runner()->PostTaskAndReply(
      FROM_HERE, base::Bind(&FrameConsumerBridge::RenderFrame,
                            weak_factory_.GetWeakPtr(), base::Passed(&frame)),
      base::Bind(&FrameConsumerBridge::OnFrameRendered,
                 weak_factory_.GetWeakPtr(), done));
}

void FrameConsumerBridge::OnFrameRendered(const base::Closure& done) {
  DCHECK(runtime_->network_task_runner()->BelongsToCurrentThread());

  if (!done.is_null())
    done.Run();
}

FrameConsumerBridge::PixelFormat FrameConsumerBridge::GetPixelFormat() {
  return FORMAT_RGBA;
}

}  // namespace remoting
