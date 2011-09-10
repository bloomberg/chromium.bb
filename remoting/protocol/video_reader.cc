// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/video_reader.h"

#include "remoting/protocol/session_config.h"
#include "remoting/protocol/protobuf_video_reader.h"
#include "remoting/protocol/rtp_video_reader.h"

namespace remoting {
namespace protocol {

VideoReader::~VideoReader() { }

// static
VideoReader* VideoReader::Create(base::MessageLoopProxy* message_loop,
                                 const SessionConfig& config) {
  const ChannelConfig& video_config = config.video_config();
  if (video_config.transport == ChannelConfig::TRANSPORT_SRTP) {
    return new RtpVideoReader(message_loop);
  } else if (video_config.transport == ChannelConfig::TRANSPORT_STREAM) {
    if (video_config.codec == ChannelConfig::CODEC_VP8) {
      return new ProtobufVideoReader(VideoPacketFormat::ENCODING_VP8);
    } else if (video_config.codec == ChannelConfig::CODEC_ZIP) {
      return new ProtobufVideoReader(VideoPacketFormat::ENCODING_ZLIB);
    } else if (video_config.codec == ChannelConfig::CODEC_VERBATIM) {
      return new ProtobufVideoReader(VideoPacketFormat::ENCODING_VERBATIM);
    }
  }
  NOTREACHED();
  return NULL;
}

}  // namespace protocol
}  // namespace remoting
