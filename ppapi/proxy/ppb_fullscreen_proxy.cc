// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_fullscreen_proxy.h"

#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace pp {
namespace proxy {

namespace {

PP_Bool IsFullscreen(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher->Send(new PpapiHostMsg_PPBFullscreen_IsFullscreen(
      INTERFACE_ID_PPB_FULLSCREEN, instance, &result));
  return result;
}

PP_Bool SetFullscreen(PP_Instance instance, PP_Bool fullscreen) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher->Send(new PpapiHostMsg_PPBFullscreen_SetFullscreen(
      INTERFACE_ID_PPB_FULLSCREEN, instance, fullscreen, &result));
  return result;
}

const PPB_Fullscreen_Dev ppb_fullscreen = {
  &IsFullscreen,
  &SetFullscreen
};

}  // namespace

PPB_Fullscreen_Proxy::PPB_Fullscreen_Proxy(Dispatcher* dispatcher,
                               const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Fullscreen_Proxy::~PPB_Fullscreen_Proxy() {
}

const void* PPB_Fullscreen_Proxy::GetSourceInterface() const {
  return &ppb_fullscreen;
}

InterfaceID PPB_Fullscreen_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_FULLSCREEN;
}

bool PPB_Fullscreen_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Fullscreen_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFullscreen_IsFullscreen,
                        OnMsgIsFullscreen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFullscreen_SetFullscreen,
                        OnMsgSetFullscreen)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw): handle bad messages!
  return handled;
}

void PPB_Fullscreen_Proxy::OnMsgIsFullscreen(PP_Instance instance,
                                             PP_Bool* result) {
  *result = ppb_fullscreen_target()->IsFullscreen(instance);
}

void PPB_Fullscreen_Proxy::OnMsgSetFullscreen(PP_Instance instance,
                                              PP_Bool fullscreen,
                                              PP_Bool* result) {
  *result = ppb_fullscreen_target()->SetFullscreen(instance, fullscreen);
}

}  // namespace proxy
}  // namespace pp
