// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_dispatcher_base.h"

#include "base/bind.h"
#include "remoting/protocol/p2p_stream_socket.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/stream_channel_factory.h"
#include "remoting/protocol/transport.h"

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

void ChannelDispatcherBase::Init(Session* session,
                                 const ChannelConfig& config,
                                 EventHandler* event_handler) {
  DCHECK(session);
  switch (config.transport) {
    case ChannelConfig::TRANSPORT_MUX_STREAM:
      channel_factory_ =
          session->GetTransport()->GetMultiplexedChannelFactory();
      break;

    case ChannelConfig::TRANSPORT_QUIC_STREAM:
      channel_factory_ = session->GetQuicChannelFactory();
      break;

    case ChannelConfig::TRANSPORT_STREAM:
      channel_factory_ = session->GetTransport()->GetStreamChannelFactory();
      break;

    default:
      LOG(FATAL) << "Unknown transport type: " << config.transport;
  }

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
  channel_ = socket.Pass();
  writer_.Init(
      base::Bind(&P2PStreamSocket::Write, base::Unretained(channel_.get())),
      base::Bind(&ChannelDispatcherBase::OnReadWriteFailed,
                 base::Unretained(this)));
  reader_.StartReading(channel_.get(),
                       base::Bind(&ChannelDispatcherBase::OnReadWriteFailed,
                                  base::Unretained(this)));

  event_handler_->OnChannelInitialized(this);
}

void ChannelDispatcherBase::OnReadWriteFailed(int error) {
  event_handler_->OnChannelError(this, CHANNEL_CONNECTION_ERROR);
}

}  // namespace protocol
}  // namespace remoting
