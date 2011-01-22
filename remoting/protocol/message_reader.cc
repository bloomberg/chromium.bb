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
      message_loop_(NULL),
      read_pending_(false),
      pending_messages_(0),
      closed_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &MessageReader::OnRead)) {
}

MessageReader::~MessageReader() {
  CHECK_EQ(pending_messages_, 0);
}

void MessageReader::Init(net::Socket* socket,
                         MessageReceivedCallback* callback) {
  message_received_callback_.reset(callback);
  DCHECK(socket);
  socket_ = socket;
  message_loop_ = MessageLoop::current();
  DoRead();
}

void MessageReader::DoRead() {
  DCHECK(!read_pending_);

  // Don't try to read again if there is another read pending or we
  // have messages that we haven't finished processing yet.
  while (!closed_ && !read_pending_ && pending_messages_ == 0) {
    read_buffer_ = new net::IOBuffer(kReadBufferSize);
    int result = socket_->Read(
        read_buffer_, kReadBufferSize, &read_callback_);
    HandleReadResult(result);
  }
}

void MessageReader::OnRead(int result) {
  DCHECK(read_pending_);
  read_pending_ = false;

  if (!closed_) {
    HandleReadResult(result);
    DoRead();
  }
}

void MessageReader::HandleReadResult(int result) {
  if (closed_)
    return;

  if (result > 0) {
    OnDataReceived(read_buffer_, result);
  } else {
    if (result == net::ERR_CONNECTION_CLOSED) {
      closed_ = true;
    } else if (result == net::ERR_IO_PENDING) {
      read_pending_ = true;
    } else {
      LOG(ERROR) << "Read() returned error " << result;
    }
  }
}

void MessageReader::OnDataReceived(net::IOBuffer* data, int data_size) {
  message_decoder_.AddData(data, data_size);

  // Get list of all new messages first, and then call the callback
  // for all of them.
  std::vector<CompoundBuffer*> new_messages;
  while (true) {
    CompoundBuffer* buffer = message_decoder_.GetNextMessage();
    if (!buffer)
      break;
    new_messages.push_back(buffer);
  }

  pending_messages_ += new_messages.size();

  for (std::vector<CompoundBuffer*>::iterator it = new_messages.begin();
       it != new_messages.end(); ++it) {
    message_received_callback_->Run(*it, NewRunnableMethod(
        this, &MessageReader::OnMessageDone, *it));
  }
}

void MessageReader::OnMessageDone(CompoundBuffer* message) {
  delete message;
  ProcessDoneEvent();
}

void MessageReader::ProcessDoneEvent() {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &MessageReader::ProcessDoneEvent));
    return;
  }

  pending_messages_--;
  DCHECK_GE(pending_messages_, 0);

  DoRead(); // Start next read if neccessary.
}

}  // namespace protocol
}  // namespace remoting
