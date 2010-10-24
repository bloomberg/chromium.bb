// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/stream_reader.h"

#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"

namespace remoting {

// EventStreamReader class.
EventStreamReader::EventStreamReader() { }
EventStreamReader::~EventStreamReader() { }

void EventStreamReader::Init(net::Socket* socket,
                             OnMessageCallback* on_message_callback) {
  on_message_callback_.reset(on_message_callback);
  SocketReaderBase::Init(socket);
}

void EventStreamReader::OnDataReceived(net::IOBuffer* buffer, int data_size) {
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
  SocketReaderBase::Init(socket);
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
