// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_dispatcher_base.h"

#include <utility>

#include "base/bind.h"
#include "remoting/protocol/p2p_stream_socket.h"
#include "remoting/protocol/stream_channel_factory.h"

namespace remoting {
namespace protocol {

ChannelDispatcherBase::ChannelDispatcherBase(const char* channel_name)
    : channel_name_(channel_name),
      channel_factory_(nullptr),
      event_handler_(nullptr) {
}

ChannelDispatcherBase::~ChannelDispatcherBase() {
  if (channel_factory_)
    channel_factory_->CancelChannelCreation(channel_name_);
}

void ChannelDispatcherBase::Init(StreamChannelFactory* channel_factory,
                                 EventHandler* event_handler) {
  channel_factory_ = channel_factory;
  event_handler_ = event_handler;

  channel_factory_->CreateChannel(channel_name_, base::Bind(
      &ChannelDispatcherBase::OnChannelReady, base::Unretained(this)));
}

void ChannelDispatcherBase::OnChannelReady(
    scoped_ptr<P2PStreamSocket> socket) {
  if (!socket.get()) {
    event_handler_->OnChannelError(this, CHANNEL_CONNECTION_ERROR);
    return;
  }

  channel_factory_ = nullptr;
  channel_ = std::move(socket);
  writer_.Start(
      base::Bind(&P2PStreamSocket::Write, base::Unretained(channel_.get())),
      base::Bind(&ChannelDispatcherBase::OnReadWriteFailed,
                 base::Unretained(this)));
  reader_.StartReading(channel_.get(),
                       base::Bind(&ChannelDispatcherBase::OnIncomingMessage,
                                  base::Unretained(this)),
                       base::Bind(&ChannelDispatcherBase::OnReadWriteFailed,
                                  base::Unretained(this)));

  event_handler_->OnChannelInitialized(this);
}

void ChannelDispatcherBase::OnReadWriteFailed(int error) {
  event_handler_->OnChannelError(this, CHANNEL_CONNECTION_ERROR);
}

}  // namespace protocol
}  // namespace remoting
