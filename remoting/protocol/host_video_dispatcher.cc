// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/host_video_dispatcher.h"

#include "base/bind.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/message_serialization.h"

namespace remoting {
namespace protocol {

HostVideoDispatcher::HostVideoDispatcher()
    : ChannelDispatcherBase(kVideoChannelName) {
}

HostVideoDispatcher::~HostVideoDispatcher() {
}

void HostVideoDispatcher::ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                                             const base::Closure& done) {
  writer()->Write(SerializeAndFrameMessage(*packet), done);
}

}  // namespace protocol
}  // namespace remoting
