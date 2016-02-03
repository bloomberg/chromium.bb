// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/stream_message_pipe_adapter.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "remoting/base/buffered_socket_writer.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/p2p_stream_socket.h"

namespace remoting {
namespace protocol {

StreamMessagePipeAdapter::StreamMessagePipeAdapter(
    scoped_ptr<P2PStreamSocket> socket,
    const ErrorCallback& error_callback)
    : socket_(std::move(socket)),
      error_callback_(error_callback),
      writer_(new BufferedSocketWriter()) {
  DCHECK(socket_);
  DCHECK(!error_callback_.is_null());

  writer_->Start(
      base::Bind(&P2PStreamSocket::Write, base::Unretained(socket_.get())),
      base::Bind(&StreamMessagePipeAdapter::CloseOnError,
                 base::Unretained(this)));
}

StreamMessagePipeAdapter::~StreamMessagePipeAdapter() {}

void StreamMessagePipeAdapter::StartReceiving(
    const MessageReceivedCallback& callback) {
  reader_.StartReading(socket_.get(), callback,
                       base::Bind(&StreamMessagePipeAdapter::CloseOnError,
                                  base::Unretained(this)));
}

void StreamMessagePipeAdapter::Send(google::protobuf::MessageLite* message,
                                    const base::Closure& done) {
  if (writer_)
    writer_->Write(SerializeAndFrameMessage(*message), done);
}

void StreamMessagePipeAdapter::CloseOnError(int error) {
  // Stop writing on error.
  writer_.reset();

  if (!error_callback_.is_null())
    base::ResetAndReturn(&error_callback_).Run(error);
}

}  // namespace protocol
}  // namespace remoting
