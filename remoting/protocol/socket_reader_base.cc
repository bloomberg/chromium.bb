// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/socket_reader_base.h"

#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"

namespace remoting {

namespace {
int kReadBufferSize = 4096;
}  // namespace

SocketReaderBase::SocketReaderBase()
    : socket_(NULL),
      closed_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &SocketReaderBase::OnRead)) {
}

SocketReaderBase::~SocketReaderBase() { }

void SocketReaderBase::Close() {
  closed_ = true;
}

void SocketReaderBase::Init(net::Socket* socket) {
  DCHECK(socket);
  socket_ = socket;
  DoRead();
}

void SocketReaderBase::DoRead() {
  while (true) {
    read_buffer_ = new net::IOBuffer(kReadBufferSize);
    int result = socket_->Read(
        read_buffer_, kReadBufferSize, &read_callback_);
    HandleReadResult(result);
    if (result < 0)
      break;
  }
}

void SocketReaderBase::OnRead(int result) {
  if (!closed_) {
    HandleReadResult(result);
    DoRead();
  }
}

void SocketReaderBase::HandleReadResult(int result) {
  if (result > 0) {
    OnDataReceived(read_buffer_, result);
  } else {
    if (result != net::ERR_IO_PENDING)
      LOG(ERROR) << "Read() returned error " << result;
  }
}

}  // namespace remoting
