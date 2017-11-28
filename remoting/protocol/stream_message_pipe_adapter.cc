// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/stream_message_pipe_adapter.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "net/base/net_errors.h"
#include "remoting/base/buffered_socket_writer.h"
#include "remoting/base/compound_buffer.h"
#include "remoting/protocol/message_serialization.h"
#include "remoting/protocol/p2p_stream_socket.h"
#include "remoting/protocol/stream_channel_factory.h"

namespace remoting {
namespace protocol {

StreamMessagePipeAdapter::StreamMessagePipeAdapter(
    std::unique_ptr<P2PStreamSocket> socket,
    const ErrorCallback& error_callback)
    : socket_(std::move(socket)), error_callback_(error_callback) {
  DCHECK(socket_);
  DCHECK(error_callback_);
}

StreamMessagePipeAdapter::~StreamMessagePipeAdapter() = default;

void StreamMessagePipeAdapter::Start(EventHandler* event_handler) {
  DCHECK(event_handler);
  event_handler_ = event_handler;

  writer_ = base::MakeUnique<BufferedSocketWriter>();
  writer_->Start(
      base::Bind(&P2PStreamSocket::Write, base::Unretained(socket_.get())),
      base::Bind(&StreamMessagePipeAdapter::CloseOnError,
                 base::Unretained(this)));

  reader_ = base::MakeUnique<MessageReader>();
  reader_->StartReading(socket_.get(),
                        base::Bind(&EventHandler::OnMessageReceived,
                                   base::Unretained(event_handler_)),
                        base::Bind(&StreamMessagePipeAdapter::CloseOnError,
                                   base::Unretained(this)));

  event_handler_->OnMessagePipeOpen();
}

void StreamMessagePipeAdapter::Send(google::protobuf::MessageLite* message,
                                    const base::Closure& done) {
  if (writer_)
    writer_->Write(SerializeAndFrameMessage(*message), done);
}

void StreamMessagePipeAdapter::CloseOnError(int error) {
  // Stop reading and writing on error.
  writer_.reset();
  reader_.reset();

  if (error == 0) {
    event_handler_->OnMessagePipeClosed();
  } else if (error_callback_) {
    base::ResetAndReturn(&error_callback_).Run(error);
  }
}

StreamMessageChannelFactoryAdapter::StreamMessageChannelFactoryAdapter(
    StreamChannelFactory* stream_channel_factory,
    const ErrorCallback& error_callback)
    : stream_channel_factory_(stream_channel_factory),
      error_callback_(error_callback) {}

StreamMessageChannelFactoryAdapter::~StreamMessageChannelFactoryAdapter() =
    default;

void StreamMessageChannelFactoryAdapter::CreateChannel(
    const std::string& name,
    const ChannelCreatedCallback& callback) {
  stream_channel_factory_->CreateChannel(
      name, base::Bind(&StreamMessageChannelFactoryAdapter::OnChannelCreated,
                       base::Unretained(this), callback));
}

void StreamMessageChannelFactoryAdapter::CancelChannelCreation(
    const std::string& name) {
  stream_channel_factory_->CancelChannelCreation(name);
}

void StreamMessageChannelFactoryAdapter::OnChannelCreated(
    const ChannelCreatedCallback& callback,
    std::unique_ptr<P2PStreamSocket> socket) {
  if (!socket) {
    error_callback_.Run(net::ERR_FAILED);
    return;
  }
  callback.Run(base::MakeUnique<StreamMessagePipeAdapter>(std::move(socket),
                                                          error_callback_));
}

}  // namespace protocol
}  // namespace remoting
