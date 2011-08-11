// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/protobuf_video_writer.h"

#include "base/bind.h"
#include "base/task.h"
#include "net/socket/stream_socket.h"
#include "remoting/base/constants.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/rtp_writer.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

ProtobufVideoWriter::ProtobufVideoWriter() { }

ProtobufVideoWriter::~ProtobufVideoWriter() { }

void ProtobufVideoWriter::Init(protocol::Session* session,
                               const InitializedCallback& callback) {
  initialized_callback_ = callback;

  session->CreateStreamChannel(
      kVideoChannelName,
      base::Bind(&ProtobufVideoWriter::OnChannelReady, base::Unretained(this)));
}

void ProtobufVideoWriter::OnChannelReady(const std::string& name,
                                         net::StreamSocket* socket) {
  DCHECK_EQ(name, std::string(kVideoChannelName));
  if (!socket) {
    initialized_callback_.Run(false);
    return;
  }

  DCHECK(!channel_.get());
  channel_.reset(socket);
  buffered_writer_ = new BufferedSocketWriter();
  // TODO(sergeyu): Provide WriteFailedCallback for the buffered writer.
  buffered_writer_->Init(socket, NULL);

  initialized_callback_.Run(true);
}

void ProtobufVideoWriter::Close() {
  buffered_writer_->Close();
  channel_.reset();
}

void ProtobufVideoWriter::ProcessVideoPacket(const VideoPacket* packet,
                                             Task* done) {
  buffered_writer_->Write(SerializeAndFrameMessage(*packet), done);
}

int ProtobufVideoWriter::GetPendingPackets() {
  return buffered_writer_->GetBufferChunks();
}

}  // namespace protocol
}  // namespace remoting
