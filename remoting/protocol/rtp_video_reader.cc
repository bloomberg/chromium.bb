// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rtp_video_reader.h"

#include "base/task.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

RtpVideoReader::RtpVideoReader() { }
RtpVideoReader::~RtpVideoReader() { }

void RtpVideoReader::Init(protocol::Session* session, VideoStub* video_stub) {
  rtp_reader_.Init(session->video_rtp_channel(),
                   NewCallback(this, &RtpVideoReader::OnRtpPacket));
  video_stub_ = video_stub;
}

void RtpVideoReader::Close() {
  rtp_reader_.Close();
}

void RtpVideoReader::OnRtpPacket(const RtpPacket& rtp_packet) {
  VideoPacket* packet = new VideoPacket();

  packet->mutable_data()->resize(rtp_packet.payload().total_bytes());
  rtp_packet.payload().CopyTo(
      const_cast<char*>(packet->mutable_data()->data()),
      packet->data().size());

  packet->mutable_format()->set_encoding(VideoPacketFormat::ENCODING_VP8);
  packet->set_flags(rtp_packet.header().marker ? VideoPacket::LAST_PACKET : 0);

  video_stub_->ProcessVideoPacket(packet, new DeleteTask<VideoPacket>(packet));
}

}  // namespace protocol
}  // namespace remoting
