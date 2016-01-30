// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/host_video_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/video_feedback_stub.h"

namespace remoting {
namespace protocol {

HostVideoDispatcher::HostVideoDispatcher()
    : ChannelDispatcherBase(kVideoChannelName),
      parser_(
          base::Bind(&HostVideoDispatcher::OnVideoAck, base::Unretained(this)),
          reader()),
      video_feedback_stub_(nullptr) {}

HostVideoDispatcher::~HostVideoDispatcher() {}

void HostVideoDispatcher::ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                                             const base::Closure& done) {
  writer()->Write(SerializeAndFrameMessage(*packet), done);
}

void HostVideoDispatcher::OnVideoAck(scoped_ptr<VideoAck> ack) {
  if (video_feedback_stub_)
    video_feedback_stub_->ProcessVideoAck(std::move(ack));
}

}  // namespace protocol
}  // namespace remoting
