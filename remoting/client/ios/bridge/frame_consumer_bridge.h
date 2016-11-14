// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IOS_BRIDGE_FRAME_CONSUMER_BRIDGE_H_
#define REMOTING_CLIENT_IOS_BRIDGE_FRAME_CONSUMER_BRIDGE_H_

#include <list>
#include <memory>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "remoting/client/ios/bridge/client_instance.h"
#include "remoting/protocol/frame_consumer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

class ClientInstance;

// TODO(nicholss): It should be possible to use remoting/client/
// dual_buffer_frame_consumer.h instead of this class.
class FrameConsumerBridge : public protocol::FrameConsumer {
 public:
  typedef base::Callback<void(webrtc::DesktopFrame* buffer)> OnFrameCallback;

  // A callback is provided to return frame updates asynchronously.
  explicit FrameConsumerBridge(ClientInstance* client_instance,
                               const OnFrameCallback callback);

  ~FrameConsumerBridge() override;

  std::unique_ptr<webrtc::DesktopFrame> AllocateFrame(
      const webrtc::DesktopSize& size) override;

  void DrawFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                 const base::Closure& done) override;

  PixelFormat GetPixelFormat() override;

 private:
  void RenderFrame(std::unique_ptr<webrtc::DesktopFrame> frame);

  void OnFrameRendered(const base::Closure& done);

  OnFrameCallback callback_;

  ClientInstance* runtime_;

  webrtc::DesktopSize view_size_;

  base::WeakPtrFactory<FrameConsumerBridge> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FrameConsumerBridge);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_IOS_BRIDGE_FRAME_CONSUMER_BRIDGE_H_
