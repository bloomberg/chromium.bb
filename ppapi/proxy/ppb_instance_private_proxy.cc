// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_instance_private_proxy.h"

#include "ppapi/c/pp_var.h"
#include "ppapi/c/private/ppb_instance_private.h"
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
  dispatcher->Send(new PpapiHostMsg_PPBInstancePrivate_GetWindowObject(
      INTERFACE_ID_PPB_INSTANCE_PRIVATE, instance, &result));
  return result.Return(dispatcher);
}

PP_Var GetOwnerElementObject(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBInstancePrivate_GetOwnerElementObject(
          INTERFACE_ID_PPB_INSTANCE_PRIVATE, instance, &result));
  return result.Return(dispatcher);
}

PP_Var ExecuteScript(PP_Instance instance, PP_Var script, PP_Var* exception) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedException se(dispatcher, exception);
  if (se.IsThrown())
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBInstancePrivate_ExecuteScript(
      INTERFACE_ID_PPB_INSTANCE_PRIVATE, instance,
      SerializedVarSendInput(dispatcher, script), &se, &result));
  return result.Return(dispatcher);
}

const PPB_Instance_Private instance_private_interface = {
  &GetWindowObject,
  &GetOwnerElementObject,
  &ExecuteScript
};

InterfaceProxy* CreateInstancePrivateProxy(Dispatcher* dispatcher,
                                           const void* target_interface) {
  return new PPB_Instance_Private_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Instance_Private_Proxy::PPB_Instance_Private_Proxy(
    Dispatcher* dispatcher, const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Instance_Private_Proxy::~PPB_Instance_Private_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Instance_Private_Proxy::GetInfo() {
  static const Info info = {
    &instance_private_interface,
    PPB_INSTANCE_PRIVATE_INTERFACE,
    INTERFACE_ID_PPB_INSTANCE_PRIVATE,
    false,
    &CreateInstancePrivateProxy,
  };
  return &info;
}

bool PPB_Instance_Private_Proxy::OnMessageReceived(const IPC::Message& msg) {
  // Prevent the dispatcher from going away during a call to ExecuteScript.
  // This must happen OUTSIDE of ExecuteScript since the SerializedVars use
  // the dispatcher upon return of the function (converting the
  // SerializedVarReturnValue/OutParam to a SerializedVar in the destructor).
  ScopedModuleReference death_grip(dispatcher());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Instance_Private_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstancePrivate_GetWindowObject,
                        OnMsgGetWindowObject)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstancePrivate_GetOwnerElementObject,
                        OnMsgGetOwnerElementObject)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstancePrivate_ExecuteScript,
                        OnMsgExecuteScript)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_Instance_Private_Proxy::OnMsgGetWindowObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  result.Return(dispatcher(),
                ppb_instance_private_target()->GetWindowObject(instance));
}

void PPB_Instance_Private_Proxy::OnMsgGetOwnerElementObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  result.Return(dispatcher(),
                ppb_instance_private_target()->GetOwnerElementObject(instance));
}

void PPB_Instance_Private_Proxy::OnMsgExecuteScript(
    PP_Instance instance,
    SerializedVarReceiveInput script,
    SerializedVarOutParam out_exception,
    SerializedVarReturnValue result) {
  if (dispatcher()->IsPlugin())
    NOTREACHED();
  else
    static_cast<HostDispatcher*>(dispatcher())->set_allow_plugin_reentrancy();

  result.Return(dispatcher(), ppb_instance_private_target()->ExecuteScript(
      instance,
      script.Get(dispatcher()),
      out_exception.OutParam(dispatcher())));
}

}  // namespace proxy
}  // namespace pp
