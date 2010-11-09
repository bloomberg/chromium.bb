// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/protobuf_video_reader.h"

#include "base/task.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

ProtobufVideoReader::ProtobufVideoReader(VideoPacketFormat::Encoding encoding)
    : encoding_(encoding) {
}

ProtobufVideoReader::~ProtobufVideoReader() { }

void ProtobufVideoReader::Init(protocol::Session* session,
                               VideoStub* video_stub) {
  reader_.Init<VideoPacket>(
      session->video_channel(),
      NewCallback(this, &ProtobufVideoReader::OnNewData));
  video_stub_ = video_stub;
}

void ProtobufVideoReader::Close() {
  reader_.Close();
}

void ProtobufVideoReader::OnNewData(VideoPacket* packet) {
  video_stub_->ProcessVideoPacket(packet, new DeleteTask<VideoPacket>(packet));
}

}  // namespace protocol
}  // namespace remoting
