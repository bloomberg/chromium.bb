// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/protobuf_video_writer.h"

#include "base/task.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/rtp_writer.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

ProtobufVideoWriter::ProtobufVideoWriter() { }

ProtobufVideoWriter::~ProtobufVideoWriter() { }

void ProtobufVideoWriter::Init(protocol::Session* session) {
  buffered_writer_ = new BufferedSocketWriter();
  // TODO(sergeyu): Provide WriteFailedCallback for the buffered writer.
  buffered_writer_->Init(session->video_channel(), NULL);
}

void ProtobufVideoWriter::Close() {
  buffered_writer_->Close();
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
