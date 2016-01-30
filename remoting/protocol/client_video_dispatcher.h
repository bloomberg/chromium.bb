// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_CLIENT_VIDEO_DISPATCHER_H_
#define REMOTING_PROTOCOL_CLIENT_VIDEO_DISPATCHER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/protobuf_message_parser.h"

namespace remoting {
namespace protocol {

class VideoStub;

class ClientVideoDispatcher : public ChannelDispatcherBase {
 public:
  explicit ClientVideoDispatcher(VideoStub* video_stub);
  ~ClientVideoDispatcher() override;

 private:
  struct PendingFrame;
  typedef std::list<PendingFrame> PendingFramesList;

  void ProcessVideoPacket(scoped_ptr<VideoPacket> video_packet);

  // Callback for VideoStub::ProcessVideoPacket().
  void OnPacketDone(PendingFramesList::iterator pending_frame);

  PendingFramesList pending_frames_;

  VideoStub* video_stub_;
  ProtobufMessageParser<VideoPacket> parser_;

  base::WeakPtrFactory<ClientVideoDispatcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ClientVideoDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIENT_VIDEO_DISPATCHER_H_
