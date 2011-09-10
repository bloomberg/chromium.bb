// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/video_writer.h"

#include "remoting/protocol/session_config.h"
#include "remoting/protocol/protobuf_video_writer.h"
#include "remoting/protocol/rtp_video_writer.h"

namespace remoting {
namespace protocol {

VideoWriter::~VideoWriter() { }

// static
VideoWriter* VideoWriter::Create(base::MessageLoopProxy* message_loop,
                                 const SessionConfig& config) {
  const ChannelConfig& video_config = config.video_config();
  if (video_config.transport == ChannelConfig::TRANSPORT_SRTP) {
    return new RtpVideoWriter(message_loop);
  } else if (video_config.transport == ChannelConfig::TRANSPORT_STREAM) {
    return new ProtobufVideoWriter(message_loop);
  }
  return NULL;
}

}  // namespace protocol
}  // namespace remoting
