// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/protobuf_video_reader.h"

#include "base/bind.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/channel_factory.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

ProtobufVideoReader::ProtobufVideoReader(VideoPacketFormat::Encoding encoding)
    : encoding_(encoding),
      channel_factory_(NULL),
      video_stub_(NULL) {
}

ProtobufVideoReader::~ProtobufVideoReader() {
  if (channel_factory_)
    channel_factory_->CancelChannelCreation(kVideoChannelName);
}

void ProtobufVideoReader::Init(protocol::Session* session,
                               VideoStub* video_stub,
                               const InitializedCallback& callback) {
  channel_factory_ = session->GetTransportChannelFactory();
  initialized_callback_ = callback;
  video_stub_ = video_stub;

  channel_factory_->CreateStreamChannel(
      kVideoChannelName,
      base::Bind(&ProtobufVideoReader::OnChannelReady, base::Unretained(this)));
}

bool ProtobufVideoReader::is_connected() {
  return channel_.get() != NULL;
}

void ProtobufVideoReader::OnChannelReady(scoped_ptr<net::StreamSocket> socket) {
  if (!socket.get()) {
    initialized_callback_.Run(false);
    return;
  }

  DCHECK(!channel_.get());
  channel_ = socket.Pass();
  reader_.Init(channel_.get(), base::Bind(&ProtobufVideoReader::OnNewData,
                                          base::Unretained(this)));
  initialized_callback_.Run(true);
}

void ProtobufVideoReader::OnNewData(scoped_ptr<VideoPacket> packet,
                                    const base::Closure& done_task) {
  video_stub_->ProcessVideoPacket(packet.Pass(), done_task);
}

}  // namespace protocol
}  // namespace remoting
