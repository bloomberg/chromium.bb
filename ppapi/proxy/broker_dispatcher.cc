// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/broker_dispatcher.h"

#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

BrokerDispatcher::BrokerDispatcher(base::ProcessHandle remote_process_handle,
                                   PP_ConnectInstance_Func connect_instance)
    : ProxyChannel(remote_process_handle) {
  // TODO(ddorwin): Do something with connect_instance.
}

BrokerDispatcher::~BrokerDispatcher() {
}

bool BrokerDispatcher::InitBrokerWithChannel(
    ProxyChannel::Delegate* delegate,
    const IPC::ChannelHandle& channel_handle,
    bool is_client) {
  return ProxyChannel::InitWithChannel(delegate, channel_handle, is_client);
}

bool BrokerDispatcher::OnMessageReceived(const IPC::Message& msg) {
  // Control messages.
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    bool handled = true;
    // TODO(ddorwin): Implement. Don't build empty block - fails on Windows.
#if 0
    IPC_BEGIN_MESSAGE_MAP(BrokerDispatcher, msg)
      // IPC_MESSAGE_FORWARD(PpapiMsg_ConnectToInstance)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
#endif
    return handled;
  }
  return false;
}

}  // namespace proxy
}  // namespace pp
