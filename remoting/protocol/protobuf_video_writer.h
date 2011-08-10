// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PROTOBUF_VIDEO_WRITER_H_
#define REMOTING_PROTOCOL_PROTOBUF_VIDEO_WRITER_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "remoting/protocol/video_writer.h"

namespace remoting {
namespace protocol {

class BufferedSocketWriter;
class Session;

class ProtobufVideoWriter : public VideoWriter {
 public:
  ProtobufVideoWriter();
  virtual ~ProtobufVideoWriter();

  // VideoWriter interface.
  virtual void Init(protocol::Session* session) OVERRIDE;
  virtual void Close() OVERRIDE;

  // VideoStub interface.
  virtual void ProcessVideoPacket(const VideoPacket* packet,
                                  Task* done) OVERRIDE;
  virtual int GetPendingPackets() OVERRIDE;

 private:
  scoped_refptr<BufferedSocketWriter> buffered_writer_;

  DISALLOW_COPY_AND_ASSIGN(ProtobufVideoWriter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PROTOBUF_VIDEO_WRITER_H_
