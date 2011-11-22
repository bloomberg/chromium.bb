// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PROTOBUF_VIDEO_WRITER_H_
#define REMOTING_PROTOCOL_PROTOBUF_VIDEO_WRITER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/video_writer.h"

namespace base {
class MessageLoopProxy;
}  // namespace base

namespace net {
class StreamSocket;
}  // namespace net

namespace remoting {
namespace protocol {

class BufferedSocketWriter;
class Session;

class ProtobufVideoWriter : public VideoWriter {
 public:
  ProtobufVideoWriter(base::MessageLoopProxy* message_loop);
  virtual ~ProtobufVideoWriter();

  // VideoWriter interface.
  virtual void Init(protocol::Session* session,
                    const InitializedCallback& callback) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual bool is_connected() OVERRIDE;

  // VideoStub interface.
  virtual void ProcessVideoPacket(const VideoPacket* packet,
                                  const base::Closure& done) OVERRIDE;
  virtual int GetPendingPackets() OVERRIDE;

 private:
  void OnChannelReady(net::StreamSocket* socket);

  Session* session_;

  InitializedCallback initialized_callback_;

  // TODO(sergeyu): Remove |channel_| and let |buffered_writer_| own it.
  scoped_ptr<net::StreamSocket> channel_;

  scoped_refptr<BufferedSocketWriter> buffered_writer_;

  DISALLOW_COPY_AND_ASSIGN(ProtobufVideoWriter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PROTOBUF_VIDEO_WRITER_H_
