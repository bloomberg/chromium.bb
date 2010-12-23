// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of HostStub using sockets created from jingle connection.
// It sends messages through the socket after serializing it.
//
// Object of this class can only be created by ConnectionToHost.
//
// This class can be used on any thread.

#ifndef REMOTING_PROTOCOL_HOST_STUB_IMPL_H_
#define REMOTING_PROTOCOL_HOST_STUB_IMPL_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "remoting/protocol/host_stub.h"

class Task;

namespace net {
class Socket;
}  // namespace net

namespace remoting {
namespace protocol {

class BufferedSocketWriter;

class HostControlSender : public HostStub {
 public:
  // Create a stub using a socket.
  explicit HostControlSender(net::Socket* socket);
  virtual ~HostControlSender();

  virtual void SuggestResolution(
      const SuggestResolutionRequest* msg, Task* done);
  virtual void BeginSessionRequest(
      const LocalLoginCredentials* credentials, Task* done);

 private:
  // Buffered socket writer holds the serialized message and send it on the
  // right thread.
  scoped_refptr<BufferedSocketWriter> buffered_writer_;

  DISALLOW_COPY_AND_ASSIGN(HostControlSender);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_STUB_IMPL_H_
