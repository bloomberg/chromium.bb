// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_messaging_proxy.h"

#include <algorithm>

#include "ppapi/c/ppp_messaging.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"

namespace ppapi {
namespace proxy {

namespace {

void HandleMessage(PP_Instance instance, PP_Var message_data) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher || (message_data.type == PP_VARTYPE_OBJECT)) {
    // The dispatcher should always be valid, and the browser should never send
    // an 'object' var over PPP_Messaging.
    NOTREACHED();
    return;
  }

  dispatcher->Send(new PpapiMsg_PPPMessaging_HandleMessage(
      INTERFACE_ID_PPP_MESSAGING,
      instance,
      SerializedVarSendInput(dispatcher, message_data)));
}

static const PPP_Messaging messaging_interface = {
  &HandleMessage
};

InterfaceProxy* CreateMessagingProxy(Dispatcher* dispatcher,
                                     const void* target_interface) {
  return new PPP_Messaging_Proxy(dispatcher, target_interface);
}

}  // namespace

PPP_Messaging_Proxy::PPP_Messaging_Proxy(Dispatcher* dispatcher,
                                         const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPP_Messaging_Proxy::~PPP_Messaging_Proxy() {
}

// static
const InterfaceProxy::Info* PPP_Messaging_Proxy::GetInfo() {
  static const Info info = {
    &messaging_interface,
    PPP_MESSAGING_INTERFACE,
    INTERFACE_ID_PPP_MESSAGING,
    false,
    &CreateMessagingProxy,
  };
  return &info;
}

bool PPP_Messaging_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_Messaging_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPMessaging_HandleMessage,
                        OnMsgHandleMessage)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPP_Messaging_Proxy::OnMsgHandleMessage(
    PP_Instance instance, SerializedVarReceiveInput message_data) {
  PP_Var received_var(message_data.Get(dispatcher()));
  // SerializedVarReceiveInput will decrement the reference count, but we want
  // to give the recipient a reference.
  PluginResourceTracker::GetInstance()->var_tracker().AddRefVar(received_var);
  ppp_messaging_target()->HandleMessage(instance, received_var);
}

}  // namespace proxy
}  // namespace ppapi
