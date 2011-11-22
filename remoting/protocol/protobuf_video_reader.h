// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PROTOBUF_VIDEO_READER_H_
#define REMOTING_PROTOCOL_PROTOBUF_VIDEO_READER_H_

#include "base/compiler_specific.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/message_reader.h"
#include "remoting/protocol/video_reader.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace remoting {
namespace protocol {

class Session;

class ProtobufVideoReader : public VideoReader {
 public:
  ProtobufVideoReader(VideoPacketFormat::Encoding encoding);
  virtual ~ProtobufVideoReader();

  // VideoReader interface.
  virtual void Init(protocol::Session* session,
                    VideoStub* video_stub,
                    const InitializedCallback& callback) OVERRIDE;
  virtual bool is_connected() OVERRIDE;

 private:
  void OnChannelReady(net::StreamSocket* socket);
  void OnNewData(VideoPacket* packet, const base::Closure& done_task);

  Session* session_;

  InitializedCallback initialized_callback_;

  VideoPacketFormat::Encoding encoding_;

  // TODO(sergeyu): Remove |channel_| and let |reader_| own it.
  scoped_ptr<net::StreamSocket> channel_;

  ProtobufMessageReader<VideoPacket> reader_;

  // The stub that processes all received packets.
  VideoStub* video_stub_;

  DISALLOW_COPY_AND_ASSIGN(ProtobufVideoReader);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PROTOBUF_VIDEO_READER_H_
