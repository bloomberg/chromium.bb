// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/client_video_dispatcher.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/message_pipe.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/video_stub.h"

namespace remoting {
namespace protocol {

struct ClientVideoDispatcher::PendingFrame {
  PendingFrame(int frame_id)
      : frame_id(frame_id),
        done(false) {}
  int frame_id;
  bool done;
};

ClientVideoDispatcher::ClientVideoDispatcher(VideoStub* video_stub)
    : ChannelDispatcherBase(kVideoChannelName),
      video_stub_(video_stub),
      weak_factory_(this) {}
ClientVideoDispatcher::~ClientVideoDispatcher() {}

void ClientVideoDispatcher::OnIncomingMessage(
    scoped_ptr<CompoundBuffer> message) {
  scoped_ptr<VideoPacket> video_packet =
      ParseMessage<VideoPacket>(message.get());
  if (!video_packet)
    return;

  int frame_id = video_packet->frame_id();

  if (!video_packet->has_frame_id()) {
    video_stub_->ProcessVideoPacket(std::move(video_packet),
                                    base::Bind(&base::DoNothing));
    return;
  }

  PendingFramesList::iterator pending_frame =
      pending_frames_.insert(pending_frames_.end(), PendingFrame(frame_id));

  video_stub_->ProcessVideoPacket(
      std::move(video_packet),
      base::Bind(&ClientVideoDispatcher::OnPacketDone,
                 weak_factory_.GetWeakPtr(), pending_frame));
}

void ClientVideoDispatcher::OnPacketDone(
    PendingFramesList::iterator pending_frame) {
  // Mark the frame as done.
  DCHECK(!pending_frame->done);
  pending_frame->done = true;

  // Send VideoAck for all packets in the head of the queue that have finished
  // rendering.
  while (!pending_frames_.empty() && pending_frames_.front().done) {
    VideoAck ack_message;
    ack_message.set_frame_id(pending_frames_.front().frame_id);
    message_pipe()->Send(&ack_message, base::Closure());
    pending_frames_.pop_front();
  }
}

}  // namespace protocol
}  // namespace remoting
