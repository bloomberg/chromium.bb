// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/protobuf_video_writer.h"

#include "base/bind.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/channel_factory.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

ProtobufVideoWriter::ProtobufVideoWriter()
    : channel_factory_(NULL) {
}

ProtobufVideoWriter::~ProtobufVideoWriter() {
  Close();
}

void ProtobufVideoWriter::Init(protocol::Session* session,
                               const InitializedCallback& callback) {
  channel_factory_ = session->GetTransportChannelFactory();
  initialized_callback_ = callback;

  channel_factory_->CreateStreamChannel(
      kVideoChannelName,
      base::Bind(&ProtobufVideoWriter::OnChannelReady, base::Unretained(this)));
}

void ProtobufVideoWriter::OnChannelReady(scoped_ptr<net::StreamSocket> socket) {
  if (!socket.get()) {
    initialized_callback_.Run(false);
    return;
  }

  DCHECK(!channel_.get());
  channel_ = socket.Pass();
  // TODO(sergeyu): Provide WriteFailedCallback for the buffered writer.
  buffered_writer_.Init(
      channel_.get(), BufferedSocketWriter::WriteFailedCallback());

  initialized_callback_.Run(true);
}

void ProtobufVideoWriter::Close() {
  buffered_writer_.Close();
  channel_.reset();
  if (channel_factory_) {
    channel_factory_->CancelChannelCreation(kVideoChannelName);
    channel_factory_ = NULL;
  }
}

bool ProtobufVideoWriter::is_connected() {
  return channel_.get() != NULL;
}

void ProtobufVideoWriter::ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                                             const base::Closure& done) {
  buffered_writer_.Write(SerializeAndFrameMessage(*packet), done);
}

}  // namespace protocol
}  // namespace remoting
