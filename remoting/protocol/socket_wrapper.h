// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file should only be included by remoting/protocol/jingle_session.cc.

#ifndef REMOTING_PROTOCOL_SOCKET_WRAPPER_H_
#define REMOTING_PROTOCOL_SOCKET_WRAPPER_H_

#include "base/scoped_ptr.h"
#include "net/socket/socket.h"

namespace remoting {
namespace protocol {

// This class is used only to wrap over SSL sockets in JingleSession.
// There is a strong assumption in Chromium's code that sockets are used and
// destroyed on the same thread. However in remoting code we may destroy
// sockets on other thread. A wrapper is added between JingleSession and
// SSL sockets so we can destroy SSL sockets and still maintain valid
// references.
class SocketWrapper : public net::Socket {
 public:
  SocketWrapper(net::Socket* socket);
  virtual ~SocketWrapper();

  // net::Socket implementation.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::CompletionCallback* callback);
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::CompletionCallback* callback);
  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

  // Method to allow us to destroy the internal socket. This method must be
  // called before SocketWrapper is destroyed.
  //
  // This method must be called on the same thread where this object is
  // created.
  void Disconnect();

 private:
  // The internal socket.
  scoped_ptr<net::Socket> socket_;

  DISALLOW_COPY_AND_ASSIGN(SocketWrapper);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_SOCKET_WRAPPER_H_
