// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/channel_dispatcher_base.h"

#include "base/bind.h"
#include "net/socket/stream_socket.h"
#include "remoting/protocol/session.h"

namespace remoting {
namespace protocol {

ChannelDispatcherBase::ChannelDispatcherBase(const char* channel_name)
    : channel_name_(channel_name),
      session_(NULL) {
}

ChannelDispatcherBase::~ChannelDispatcherBase() {
  if (session_)
    session_->CancelChannelCreation(channel_name_);
}

void ChannelDispatcherBase::Init(Session* session,
                                 const InitializedCallback& callback) {
  DCHECK(session);
  session_ = session;
  initialized_callback_ = callback;

  session_->CreateStreamChannel(channel_name_, base::Bind(
      &ChannelDispatcherBase::OnChannelReady, base::Unretained(this)));
}

void ChannelDispatcherBase::OnChannelReady(net::StreamSocket* socket) {
  if (!socket) {
    initialized_callback_.Run(false);
    return;
  }

  channel_.reset(socket);

  OnInitialized();

  initialized_callback_.Run(true);
}

}  // namespace protocol
}  // namespace remoting
