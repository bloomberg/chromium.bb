// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/rtp_video_writer.h"

#include "net/base/io_buffer.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/rtp_writer.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

RtpVideoWriter::RtpVideoWriter() { }

RtpVideoWriter::~RtpVideoWriter() { }

void RtpVideoWriter::Init(protocol::Session* session) {
  rtp_writer_.Init(session->video_rtp_channel(), session->video_rtcp_channel());
}

void RtpVideoWriter::SendPacket(const VideoPacket& packet) {
  CompoundBuffer payload;
  payload.AppendCopyOf(packet.data().data(), packet.data().size());

  rtp_writer_.SendPacket(payload, packet.timestamp());
}

int RtpVideoWriter::GetPendingPackets() {
  return rtp_writer_.GetPendingPackets();
}


void RtpVideoWriter::Close() {
  rtp_writer_.Close();
}

}  // namespace protocol
}  // namespace remoting
