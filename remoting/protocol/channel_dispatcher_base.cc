// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_dispatcher_base.h"

#include "base/bind.h"
#include "net/socket/stream_socket.h"
#include "remoting/protocol/session.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/stream_channel_factory.h"

namespace remoting {
namespace protocol {

ChannelDispatcherBase::ChannelDispatcherBase(const char* channel_name)
    : channel_name_(channel_name),
      channel_factory_(NULL) {
}

ChannelDispatcherBase::~ChannelDispatcherBase() {
  if (channel_factory_)
    channel_factory_->CancelChannelCreation(channel_name_);
}

void ChannelDispatcherBase::Init(Session* session,
                                 const ChannelConfig& config,
                                 const InitializedCallback& callback) {
  DCHECK(session);
  switch (config.transport) {
    case ChannelConfig::TRANSPORT_MUX_STREAM:
      channel_factory_ = session->GetMultiplexedChannelFactory();
      break;

    case ChannelConfig::TRANSPORT_STREAM:
      channel_factory_ = session->GetTransportChannelFactory();
      break;

    default:
      NOTREACHED();
      callback.Run(false);
      return;
  }

  initialized_callback_ = callback;

  channel_factory_->CreateChannel(channel_name_, base::Bind(
      &ChannelDispatcherBase::OnChannelReady, base::Unretained(this)));
}

void ChannelDispatcherBase::OnChannelReady(
    scoped_ptr<net::StreamSocket> socket) {
  if (!socket.get()) {
    initialized_callback_.Run(false);
    return;
  }

  channel_factory_ = NULL;
  channel_ = socket.Pass();

  OnInitialized();

  initialized_callback_.Run(true);
}

}  // namespace protocol
}  // namespace remoting
