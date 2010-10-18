// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/stream_reader.h"

#include "base/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/socket.h"
#include "remoting/protocol/chromoting_connection.h"

namespace remoting {

namespace {
int kReadBufferSize = 4096;
}  // namespace

StreamReaderBase::StreamReaderBase()
    : socket_(NULL),
      closed_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          read_callback_(this, &StreamReaderBase::OnRead)) {
}

StreamReaderBase::~StreamReaderBase() { }

void StreamReaderBase::Close() {
  closed_ = true;
}

void StreamReaderBase::Init(net::Socket* socket) {
  DCHECK(socket);
  socket_ = socket;
  DoRead();
}

void StreamReaderBase::DoRead() {
  while (true) {
    read_buffer_ = new net::IOBuffer(kReadBufferSize);
    int result = socket_->Read(
        read_buffer_, kReadBufferSize, &read_callback_);
    HandleReadResult(result);
    if (result < 0)
      break;
  }
}

void StreamReaderBase::OnRead(int result) {
  if (!closed_) {
    HandleReadResult(result);
    DoRead();
  }
}

void StreamReaderBase::HandleReadResult(int result) {
  if (result > 0) {
    OnDataReceived(read_buffer_, result);
  } else {
    if (result != net::ERR_IO_PENDING)
      LOG(ERROR) << "Read() returned error " << result;
  }
}

// EventsStreamReader class.
EventsStreamReader::EventsStreamReader() { }
EventsStreamReader::~EventsStreamReader() { }

void EventsStreamReader::Init(net::Socket* socket,
                              OnMessageCallback* on_message_callback) {
  on_message_callback_.reset(on_message_callback);
  StreamReaderBase::Init(socket);
}

void EventsStreamReader::OnDataReceived(net::IOBuffer* buffer, int data_size) {
  ClientMessageList messages_list;
  messages_decoder_.ParseClientMessages(buffer, data_size, &messages_list);
  for (ClientMessageList::iterator it = messages_list.begin();
       it != messages_list.end(); ++it) {
    on_message_callback_->Run(*it);
  }
}

// VideoStreamReader class.
VideoStreamReader::VideoStreamReader() { }
VideoStreamReader::~VideoStreamReader() { }

void VideoStreamReader::Init(net::Socket* socket,
                             OnMessageCallback* on_message_callback) {
  on_message_callback_.reset(on_message_callback);
  StreamReaderBase::Init(socket);
}

void VideoStreamReader::OnDataReceived(net::IOBuffer* buffer, int data_size) {
  HostMessageList messages_list;
  messages_decoder_.ParseHostMessages(buffer, data_size, &messages_list);
  for (HostMessageList::iterator it = messages_list.begin();
       it != messages_list.end(); ++it) {
    on_message_callback_->Run(*it);
  }
}

}  // namespace remoting
