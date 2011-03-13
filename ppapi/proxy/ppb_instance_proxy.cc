// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_instance_proxy.h"

#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"

namespace pp {
namespace proxy {

namespace {

PP_Var GetWindowObject(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBInstance_GetWindowObject(
      INTERFACE_ID_PPB_INSTANCE, instance, &result));
  return result.Return(dispatcher);
}

PP_Var GetOwnerElementObject(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBInstance_GetOwnerElementObject(
          INTERFACE_ID_PPB_INSTANCE, instance, &result));
  return result.Return(dispatcher);
}

PP_Bool BindGraphics(PP_Instance instance, PP_Resource device) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_FALSE;

  PluginResource* object =
      PluginResourceTracker::GetInstance()->GetResourceObject(device);
  if (!object || object->instance() != instance)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher->Send(new PpapiHostMsg_PPBInstance_BindGraphics(
      INTERFACE_ID_PPB_INSTANCE, instance, object->host_resource(),
      &result));
  return result;
}

PP_Bool IsFullFrame(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher->Send(new PpapiHostMsg_PPBInstance_IsFullFrame(
      INTERFACE_ID_PPB_INSTANCE, instance, &result));
  return result;
}

PP_Var ExecuteScript(PP_Instance instance, PP_Var script, PP_Var* exception) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedException se(dispatcher, exception);
  if (se.IsThrown())
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBInstance_ExecuteScript(
      INTERFACE_ID_PPB_INSTANCE, instance,
      SerializedVarSendInput(dispatcher, script), &se, &result));
  return result.Return(dispatcher);
}

const PPB_Instance instance_interface = {
  &GetWindowObject,
  &GetOwnerElementObject,
  &BindGraphics,
  &IsFullFrame,
  &ExecuteScript
};

InterfaceProxy* CreateInstanceProxy(Dispatcher* dispatcher,
                                    const void* target_interface) {
  return new PPB_Instance_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Instance_Proxy::PPB_Instance_Proxy(Dispatcher* dispatcher,
                                       const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Instance_Proxy::~PPB_Instance_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Instance_Proxy::GetInfo() {
  static const Info info = {
    &instance_interface,
    PPB_INSTANCE_INTERFACE,
    INTERFACE_ID_PPB_INSTANCE,
    false,
    &CreateInstanceProxy,
  };
  return &info;
}

bool PPB_Instance_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Instance_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetWindowObject,
                        OnMsgGetWindowObject)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetOwnerElementObject,
                        OnMsgGetOwnerElementObject)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_BindGraphics,
                        OnMsgBindGraphics)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_IsFullFrame,
                        OnMsgIsFullFrame)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_ExecuteScript,
                        OnMsgExecuteScript)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_Instance_Proxy::OnMsgGetWindowObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  result.Return(dispatcher(),
                ppb_instance_target()->GetWindowObject(instance));
}

void PPB_Instance_Proxy::OnMsgGetOwnerElementObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  result.Return(dispatcher(),
                ppb_instance_target()->GetOwnerElementObject(instance));
}

void PPB_Instance_Proxy::OnMsgBindGraphics(PP_Instance instance,
                                           HostResource device,
                                           PP_Bool* result) {
  *result = ppb_instance_target()->BindGraphics(instance,
                                                device.host_resource());
}

void PPB_Instance_Proxy::OnMsgIsFullFrame(PP_Instance instance,
                                          PP_Bool* result) {
  *result = ppb_instance_target()->IsFullFrame(instance);
}

void PPB_Instance_Proxy::OnMsgExecuteScript(
    PP_Instance instance,
    SerializedVarReceiveInput script,
    SerializedVarOutParam out_exception,
    SerializedVarReturnValue result) {
  if (dispatcher()->IsPlugin())
    NOTREACHED();
  else
    static_cast<HostDispatcher*>(dispatcher())->set_allow_plugin_reentrancy();

  result.Return(dispatcher(), ppb_instance_target()->ExecuteScript(
      instance,
      script.Get(dispatcher()),
      out_exception.OutParam(dispatcher())));
}

}  // namespace proxy
}  // namespace pp
