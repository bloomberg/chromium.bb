// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/socket_wrapper.h"

#include "base/logging.h"
#include "net/base/net_errors.h"

namespace remoting {
namespace protocol {

SocketWrapper::SocketWrapper(net::Socket* socket)
    : socket_(socket) {
}

SocketWrapper::~SocketWrapper() {
  DCHECK(!socket_.get());
}

int SocketWrapper::Read(net::IOBuffer* buf, int buf_len,
                        net::CompletionCallback* callback) {
  if (!socket_.get())
    return net::ERR_SOCKET_NOT_CONNECTED;
  return socket_->Read(buf, buf_len, callback);
}

int SocketWrapper::Write(net::IOBuffer* buf, int buf_len,
                         net::CompletionCallback* callback) {
  if (!socket_.get())
    return net::ERR_SOCKET_NOT_CONNECTED;
  return socket_->Write(buf, buf_len, callback);
}

bool SocketWrapper::SetReceiveBufferSize(int32 size) {
  if (!socket_.get())
    return false;
  return socket_->SetReceiveBufferSize(size);
}

bool SocketWrapper::SetSendBufferSize(int32 size) {
  if (!socket_.get())
    return false;
  return socket_->SetSendBufferSize(size);
}

void SocketWrapper::Disconnect() {
  socket_.reset();
}

}  // namespace protocol
}  // namespace remoting
