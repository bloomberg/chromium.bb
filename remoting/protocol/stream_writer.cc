// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/stream_writer.h"

#include "base/message_loop.h"
#include "remoting/protocol/buffered_socket_writer.h"
#include "remoting/protocol/util.h"

namespace remoting {
namespace protocol {

StreamWriterBase::StreamWriterBase()
    : socket_(NULL),
      buffered_writer_(new BufferedSocketWriter()) {
}

StreamWriterBase::~StreamWriterBase() { }

void StreamWriterBase::Init(net::Socket* socket) {
  socket_ = socket;
  buffered_writer_->Init(socket, NULL);
}

int StreamWriterBase::GetBufferSize() {
  return buffered_writer_->GetBufferSize();
}

int StreamWriterBase::GetPendingMessages() {
  return buffered_writer_->GetBufferChunks();
}

void StreamWriterBase::Close() {
  buffered_writer_->Close();
}

bool EventStreamWriter::SendMessage(const ChromotingClientMessage& message) {
  return buffered_writer_->Write(SerializeAndFrameMessage(message));
}

bool ControlStreamWriter::SendMessage(const ControlMessage& message) {
  return buffered_writer_->Write(SerializeAndFrameMessage(message));
}

}  // namespace protocol
}  // namespace remoting
