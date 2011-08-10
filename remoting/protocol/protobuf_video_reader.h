// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PROTOBUF_VIDEO_READER_H_
#define REMOTING_PROTOCOL_PROTOBUF_VIDEO_READER_H_

#include "remoting/proto/video.pb.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/video_reader.h"

namespace remoting {
namespace protocol {

class Session;

class ProtobufVideoReader : public VideoReader {
 public:
  ProtobufVideoReader(VideoPacketFormat::Encoding encoding);
  virtual ~ProtobufVideoReader();

  // VideoReader interface.
  virtual void Init(protocol::Session* session, VideoStub* video_stub);

 private:
  void OnNewData(VideoPacket* packet, Task* done_task);

  VideoPacketFormat::Encoding encoding_;

  ProtobufMessageReader<VideoPacket> reader_;

  // The stub that processes all received packets.
  VideoStub* video_stub_;

  DISALLOW_COPY_AND_ASSIGN(ProtobufVideoReader);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PROTOBUF_VIDEO_READER_H_
