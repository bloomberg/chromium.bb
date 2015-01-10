// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_VIDEO_DISPATCHER_H_
#define REMOTING_PROTOCOL_HOST_VIDEO_DISPATCHER_H_

#include <string>

#include "base/compiler_specific.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/video_stub.h"

namespace remoting {
namespace protocol {

class HostVideoDispatcher : public ChannelDispatcherBase, public VideoStub {
 public:
  HostVideoDispatcher();
  ~HostVideoDispatcher() override;

  // VideoStub interface.
  void ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                          const base::Closure& done) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostVideoDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_VIDEO_DISPATCHER_H_
