// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/video_writer.h"

#include "remoting/protocol/chromotocol_config.h"
#include "remoting/protocol/protobuf_video_writer.h"
#include "remoting/protocol/rtp_video_writer.h"

namespace remoting {

VideoWriter::~VideoWriter() { }

// static
VideoWriter* VideoWriter::Create(const ChromotocolConfig* config) {
  const ChannelConfig& video_config = config->video_config();
  if (video_config.transport == ChannelConfig::TRANSPORT_SRTP) {
    return new RtpVideoWriter();
  } else if (video_config.transport == ChannelConfig::TRANSPORT_STREAM) {
    return new ProtobufVideoWriter();
  }
  return NULL;
}

}  // namespace remoting
