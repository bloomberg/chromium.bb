// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/video_reader.h"

#include "remoting/protocol/session_config.h"
#include "remoting/protocol/protobuf_video_reader.h"

namespace remoting {
namespace protocol {

VideoReader::~VideoReader() { }

// static
scoped_ptr<VideoReader> VideoReader::Create(const SessionConfig& config) {
  const ChannelConfig& video_config = config.video_config();
  if (video_config.transport == ChannelConfig::TRANSPORT_STREAM) {
    if (video_config.codec == ChannelConfig::CODEC_VP8) {
      return scoped_ptr<VideoReader>(
          new ProtobufVideoReader(VideoPacketFormat::ENCODING_VP8));
    } else if (video_config.codec == ChannelConfig::CODEC_VP9) {
      return scoped_ptr<VideoReader>(
          new ProtobufVideoReader(VideoPacketFormat::ENCODING_VP9));
    } else if (video_config.codec == ChannelConfig::CODEC_ZIP) {
      return scoped_ptr<VideoReader>(
          new ProtobufVideoReader(VideoPacketFormat::ENCODING_ZLIB));
    } else if (video_config.codec == ChannelConfig::CODEC_VERBATIM) {
      return scoped_ptr<VideoReader>(
          new ProtobufVideoReader(VideoPacketFormat::ENCODING_VERBATIM));
    }
  }
  NOTREACHED();
  return scoped_ptr<VideoReader>();
}

}  // namespace protocol
}  // namespace remoting
