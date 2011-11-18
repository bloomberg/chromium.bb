// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/protobuf_video_writer.h"

#include "base/bind.h"
#include "base/task.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

ProtobufVideoWriter::ProtobufVideoWriter(base::MessageLoopProxy* message_loop)
    : session_(NULL),
      buffered_writer_(new BufferedSocketWriter(message_loop)) {
}

ProtobufVideoWriter::~ProtobufVideoWriter() {
  Close();
}

void ProtobufVideoWriter::Init(protocol::Session* session,
                               const InitializedCallback& callback) {
  session_ = session;
  initialized_callback_ = callback;

  session_->CreateStreamChannel(
      kVideoChannelName,
      base::Bind(&ProtobufVideoWriter::OnChannelReady, base::Unretained(this)));
}

void ProtobufVideoWriter::OnChannelReady(net::StreamSocket* socket) {
  if (!socket) {
    initialized_callback_.Run(false);
    return;
  }

  DCHECK(!channel_.get());
  channel_.reset(socket);
  // TODO(sergeyu): Provide WriteFailedCallback for the buffered writer.
  buffered_writer_->Init(socket, BufferedSocketWriter::WriteFailedCallback());

  initialized_callback_.Run(true);
}

void ProtobufVideoWriter::Close() {
  buffered_writer_->Close();
  channel_.reset();
  if (session_) {
    session_->CancelChannelCreation(kVideoChannelName);
    session_ = NULL;
  }
}

void ProtobufVideoWriter::ProcessVideoPacket(const VideoPacket* packet,
                                             const base::Closure& done) {
  buffered_writer_->Write(SerializeAndFrameMessage(*packet), done);
}

int ProtobufVideoWriter::GetPendingPackets() {
  return buffered_writer_->GetBufferChunks();
}

}  // namespace protocol
}  // namespace remoting
