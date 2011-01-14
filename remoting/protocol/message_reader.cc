// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/message_reader.h"

#include "base/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/proto/internal.pb.h"

namespace remoting {
namespace protocol {

static const int kReadBufferSize = 4096;

MessageReader::MessageReader()
    : socket_(NULL),
      closed_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &MessageReader::OnRead)) {
}

MessageReader::~MessageReader() {
}

void MessageReader::Init(net::Socket* socket,
                         MessageReceivedCallback* callback) {
  message_received_callback_.reset(callback);
  DCHECK(socket);
  socket_ = socket;
  DoRead();
}

void MessageReader::DoRead() {
  while (!closed_) {
    read_buffer_ = new net::IOBuffer(kReadBufferSize);
    int result = socket_->Read(
        read_buffer_, kReadBufferSize, &read_callback_);
    HandleReadResult(result);
    if (result < 0)
      break;
  }
}

void MessageReader::OnRead(int result) {
  if (!closed_) {
    HandleReadResult(result);
    DoRead();
  }
}

void MessageReader::HandleReadResult(int result) {
  if (result > 0) {
    OnDataReceived(read_buffer_, result);
  } else {
    if (result == net::ERR_CONNECTION_CLOSED) {
      closed_ = true;
    } else if (result != net::ERR_IO_PENDING) {
      LOG(ERROR) << "Read() returned error " << result;
    }
  }
}

void MessageReader::OnDataReceived(net::IOBuffer* data, int data_size) {
  message_decoder_.AddData(data, data_size);

  while (true) {
    CompoundBuffer buffer;
    if (!message_decoder_.GetNextMessage(&buffer))
      break;

    message_received_callback_->Run(&buffer);
  }
}

}  // namespace protocol
}  // namespace remoting
