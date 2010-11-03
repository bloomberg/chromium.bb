// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rtp_video_reader.h"

#include "base/task.h"
#include "remoting/protocol/session.h"

namespace remoting {

RtpVideoReader::RtpVideoReader() { }
RtpVideoReader::~RtpVideoReader() { }

void RtpVideoReader::Init(protocol::Session* session,
                          VideoStub* video_stub) {
  rtp_reader_.Init(session->video_rtp_channel(),
                   NewCallback(this, &RtpVideoReader::OnRtpPacket));
  video_stub_ = video_stub;
}

void RtpVideoReader::Close() {
  rtp_reader_.Close();
}

void RtpVideoReader::OnRtpPacket(const RtpPacket& rtp_packet) {
  VideoPacket* packet = new VideoPacket();
  packet->set_data(rtp_packet.payload, rtp_packet.payload_size);

  packet->mutable_format()->set_encoding(VideoPacketFormat::ENCODING_VP8);
  packet->set_flags(rtp_packet.header.marker ? VideoPacket::LAST_PACKET : 0);
  packet->mutable_format()->set_pixel_format(PIXEL_FORMAT_RGB32);
  packet->mutable_format()->set_x(0);
  packet->mutable_format()->set_y(0);
  packet->mutable_format()->set_width(800);
  packet->mutable_format()->set_height(600);

  video_stub_->ProcessVideoPacket(packet, new DeleteTask<VideoPacket>(packet));
}

}  // namespace remoting
