// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/dispatcher.h"

#include <string.h>  // For memset.

#include <map>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/var_serialization_rules.h"

namespace ppapi {
namespace proxy {

Dispatcher::Dispatcher(base::ProcessHandle remote_process_handle,
                       GetInterfaceFunc local_get_interface)
    : ProxyChannel(remote_process_handle),
      disallow_trusted_interfaces_(false),  // TODO(brettw) make this settable.
      local_get_interface_(local_get_interface),
      callback_tracker_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

Dispatcher::~Dispatcher() {
}

InterfaceProxy* Dispatcher::GetInterfaceProxy(InterfaceID id) {
  InterfaceProxy* proxy = proxies_[id].get();
  if (!proxy) {
    // Handle the first time for a given API by creating the proxy for it.
    InterfaceProxy::Factory factory =
        InterfaceList::GetInstance()->GetFactoryForID(id);
    if (!factory) {
      NOTREACHED();
      return NULL;
    }
    proxy = factory(this);
    DCHECK(proxy);
    proxies_[id].reset(proxy);
  }
  return proxy;
}

base::MessageLoopProxy* Dispatcher::GetIPCMessageLoop() {
  return delegate()->GetIPCMessageLoop();
}

void Dispatcher::AddIOThreadMessageFilter(
    IPC::ChannelProxy::MessageFilter* filter) {
  channel()->AddFilter(filter);
}

bool Dispatcher::OnMessageReceived(const IPC::Message& msg) {
  // Control messages.
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(Dispatcher, msg)
      IPC_MESSAGE_FORWARD(PpapiMsg_ExecuteCallback, &callback_tracker_,
                          CallbackTracker::ReceiveExecuteSerializedCallback)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  if (msg.routing_id() <= 0 || msg.routing_id() >= INTERFACE_ID_COUNT) {
    OnInvalidMessageReceived();
    return true;
  }

  InterfaceProxy* proxy = GetInterfaceProxy(
      static_cast<InterfaceID>(msg.routing_id()));
  if (!proxy) {
    NOTREACHED();
    return true;
  }
  return proxy->OnMessageReceived(msg);
}

void Dispatcher::SetSerializationRules(
    VarSerializationRules* var_serialization_rules) {
  serialization_rules_.reset(var_serialization_rules);
}

void Dispatcher::OnInvalidMessageReceived() {
}

}  // namespace proxy
}  // namespace ppapi
