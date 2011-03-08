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

PP_Bool GetScreenSize(PP_Instance instance, PP_Size* size) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher->Send(new PpapiHostMsg_PPBFullscreen_GetScreenSize(
      INTERFACE_ID_PPB_FULLSCREEN, instance, &result, size));
  return result;
}

const PPB_Fullscreen_Dev fullscreen_interface = {
  &IsFullscreen,
  &SetFullscreen,
  &GetScreenSize
};

InterfaceProxy* CreateFullscreenProxy(Dispatcher* dispatcher,
                                      const void* target_interface) {
  return new PPB_Fullscreen_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Fullscreen_Proxy::PPB_Fullscreen_Proxy(Dispatcher* dispatcher,
                               const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Fullscreen_Proxy::~PPB_Fullscreen_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Fullscreen_Proxy::GetInfo() {
  static const Info info = {
    &fullscreen_interface,
    PPB_FULLSCREEN_DEV_INTERFACE,
    INTERFACE_ID_PPB_FULLSCREEN,
    false,
    &CreateFullscreenProxy,
  };
  return &info;
}

bool PPB_Fullscreen_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Fullscreen_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFullscreen_IsFullscreen,
                        OnMsgIsFullscreen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFullscreen_SetFullscreen,
                        OnMsgSetFullscreen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFullscreen_GetScreenSize,
                        OnMsgGetScreenSize)
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

void PPB_Fullscreen_Proxy::OnMsgGetScreenSize(PP_Instance instance,
                                              PP_Bool* result,
                                              PP_Size* size) {
  *result = ppb_fullscreen_target()->GetScreenSize(instance, size);
}

}  // namespace proxy
}  // namespace pp
