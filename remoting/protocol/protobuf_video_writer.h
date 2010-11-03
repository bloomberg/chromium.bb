// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PROTOBUF_VIDEO_WRITER_H_
#define REMOTING_PROTOCOL_PROTOBUF_VIDEO_WRITER_H_

#include "base/ref_counted.h"
#include "remoting/protocol/video_writer.h"

namespace remoting {

namespace protocol {
class Session;
}  // namespace protocol

class BufferedSocketWriter;

class ProtobufVideoWriter : public VideoWriter {
 public:
  ProtobufVideoWriter();
  virtual ~ProtobufVideoWriter();

  // VideoWriter interface.
  virtual void Init(protocol::Session* session);
  virtual void SendPacket(const VideoPacket& packet);
  virtual int GetPendingPackets();
  virtual void Close();

 private:
  scoped_refptr<BufferedSocketWriter> buffered_writer_;

  DISALLOW_COPY_AND_ASSIGN(ProtobufVideoWriter);
};

}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PROTOBUF_VIDEO_WRITER_H_
