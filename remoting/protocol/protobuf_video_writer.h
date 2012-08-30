// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PROTOBUF_VIDEO_WRITER_H_
#define REMOTING_PROTOCOL_PROTOBUF_VIDEO_WRITER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/protocol/video_writer.h"

namespace net {
class StreamSocket;
}  // namespace net

namespace remoting {
namespace protocol {

class ChannelFactory;
class Session;

class ProtobufVideoWriter : public VideoWriter {
 public:
  ProtobufVideoWriter();
  virtual ~ProtobufVideoWriter();

  // VideoWriter interface.
  virtual void Init(protocol::Session* session,
                    const InitializedCallback& callback) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual bool is_connected() OVERRIDE;

  // VideoStub interface.
  virtual void ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                                  const base::Closure& done) OVERRIDE;

 private:
  void OnChannelReady(scoped_ptr<net::StreamSocket> socket);

  InitializedCallback initialized_callback_;

  ChannelFactory* channel_factory_;
  scoped_ptr<net::StreamSocket> channel_;

  BufferedSocketWriter buffered_writer_;

  DISALLOW_COPY_AND_ASSIGN(ProtobufVideoWriter);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PROTOBUF_VIDEO_WRITER_H_
