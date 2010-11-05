// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of ClientStub using sockets created from jingle connection.
// It sends messages through the socket after serializing it.
//
// Object of this class can only be created by ConnectionToClient.
//
// This class can be used on any thread.

#ifndef REMOTING_PROTOCOL_CLIENT_STUB_IMPL_H_
#define REMOTING_PROTOCOL_CLIENT_STUB_IMPL_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "remoting/protocol/client_stub.h"

class Task;

namespace net {
class Socket;
}  // namespace net

namespace remoting {
namespace protocol {

class BufferedSocketWriter;

class ClientStubImpl : public ClientStub {
 public:
  // Create a stub using a socket.
  explicit ClientStubImpl(net::Socket* socket);
  virtual ~ClientStubImpl();

  virtual void NotifyResolution(const NotifyResolutionRequest& msg,
                                Task* done) = 0;
 private:
  // Buffered socket writer holds the serialized message and send it on the
  // right thread.
  scoped_refptr<BufferedSocketWriter> buffered_writer_;

  DISALLOW_COPY_AND_ASSIGN(ClientStubImpl);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_CLIENT_STUB_IMPL_H_
