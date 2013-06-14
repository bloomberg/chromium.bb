// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/video_writer.h"

#include "remoting/protocol/session_config.h"
#include "remoting/protocol/protobuf_video_writer.h"

namespace remoting {
namespace protocol {

VideoWriter::~VideoWriter() { }

// static
scoped_ptr<VideoWriter> VideoWriter::Create(const SessionConfig& config) {
  const ChannelConfig& video_config = config.video_config();
  if (video_config.transport == ChannelConfig::TRANSPORT_STREAM) {
    return scoped_ptr<VideoWriter>(new ProtobufVideoWriter());
  }
  return scoped_ptr<VideoWriter>();
}

}  // namespace protocol
}  // namespace remoting
