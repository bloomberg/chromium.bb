// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/protobuf_video_writer.h"

#include "remoting/protocol/rtp_writer.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/util.h"

namespace remoting {

ProtobufVideoWriter::ProtobufVideoWriter() { }

ProtobufVideoWriter::~ProtobufVideoWriter() { }

void ProtobufVideoWriter::Init(protocol::Session* session) {
  buffered_writer_ = new BufferedSocketWriter();
  // TODO(sergeyu): Provide WriteFailedCallback for the buffered writer.
  buffered_writer_->Init(session->video_channel(), NULL);
}

void ProtobufVideoWriter::SendPacket(const VideoPacket& packet) {
  buffered_writer_->Write(SerializeAndFrameMessage(packet));
}

int ProtobufVideoWriter::GetPendingPackets() {
  return buffered_writer_->GetBufferChunks();
}


void ProtobufVideoWriter::Close() {
  buffered_writer_->Close();
}

}  // namespace remoting
