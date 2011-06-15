// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_instance_proxy.h"

#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterFunctionNoLock;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Instance_FunctionAPI;

namespace pp {
namespace proxy {

namespace {

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
const InterfaceProxy::Info* PPB_Instance_Proxy::GetInfo0_4() {
  static const Info info = {
    ppapi::thunk::GetPPB_Instance_0_4_Thunk(),
    PPB_INSTANCE_INTERFACE_0_4,
    INTERFACE_ID_NONE,  // 0_5 is the canonical one.
    false,
    &CreateInstanceProxy,
  };
  return &info;
}

// static
const InterfaceProxy::Info* PPB_Instance_Proxy::GetInfo0_5() {
  static const Info info = {
    ppapi::thunk::GetPPB_Instance_0_5_Thunk(),
    PPB_INSTANCE_INTERFACE_0_5,
    INTERFACE_ID_PPB_INSTANCE,
    false,
    &CreateInstanceProxy,
  };
  return &info;
}

// static
const InterfaceProxy::Info* PPB_Instance_Proxy::GetInfoPrivate() {
  static const Info info = {
    ppapi::thunk::GetPPB_Instance_Private_Thunk(),
    PPB_INSTANCE_PRIVATE_INTERFACE,
    INTERFACE_ID_NONE,  // 0_5 is the canonical one.
    false,
    &CreateInstanceProxy,
  };
  return &info;
}

// static
const InterfaceProxy::Info* PPB_Instance_Proxy::GetInfoFullscreen() {
  static const Info info = {
    ppapi::thunk::GetPPB_Fullscreen_Thunk(),
    PPB_FULLSCREEN_DEV_INTERFACE,
    INTERFACE_ID_NONE,  // 0_5 is the canonical one.
    false,
    &CreateInstanceProxy,
  };
  return &info;
}

bool PPB_Instance_Proxy::OnMessageReceived(const IPC::Message& msg) {
  // Prevent the dispatcher from going away during a call to ExecuteScript.
  // This must happen OUTSIDE of ExecuteScript since the SerializedVars use
  // the dispatcher upon return of the function (converting the
  // SerializedVarReturnValue/OutParam to a SerializedVar in the destructor).
  ScopedModuleReference death_grip(dispatcher());

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
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_SetFullscreen,
                        OnMsgSetFullscreen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetScreenSize,
                        OnMsgGetScreenSize)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

PPB_Instance_FunctionAPI* PPB_Instance_Proxy::AsPPB_Instance_FunctionAPI() {
  return this;
}

PP_Bool PPB_Instance_Proxy::BindGraphics(PP_Instance instance,
                                         PP_Resource device) {
  PluginResource* object =
      PluginResourceTracker::GetInstance()->GetResourceObject(device);
  if (!object || object->instance() != instance)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_BindGraphics(
      INTERFACE_ID_PPB_INSTANCE, instance, object->host_resource(),
      &result));
  return result;
}

PP_Bool PPB_Instance_Proxy::IsFullFrame(PP_Instance instance) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_IsFullFrame(
      INTERFACE_ID_PPB_INSTANCE, instance, &result));
  return result;
}

PP_Var PPB_Instance_Proxy::GetWindowObject(PP_Instance instance) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_GetWindowObject(
      INTERFACE_ID_PPB_INSTANCE, instance, &result));
  return result.Return(dispatcher());
}

PP_Var PPB_Instance_Proxy::GetOwnerElementObject(PP_Instance instance) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_GetOwnerElementObject(
      INTERFACE_ID_PPB_INSTANCE, instance, &result));
  return result.Return(dispatcher());
}

PP_Var PPB_Instance_Proxy::ExecuteScript(PP_Instance instance,
                                         PP_Var script,
                                         PP_Var* exception) {
  ReceiveSerializedException se(dispatcher(), exception);
  if (se.IsThrown())
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_ExecuteScript(
      INTERFACE_ID_PPB_INSTANCE, instance,
      SerializedVarSendInput(dispatcher(), script), &se, &result));
  return result.Return(dispatcher());
}

PP_Bool PPB_Instance_Proxy::IsFullscreen(PP_Instance instance) {
  InstanceData* data = static_cast<PluginDispatcher*>(dispatcher())->
      GetInstanceData(instance);
  if (!data)
    return PP_FALSE;
  return data->fullscreen;
}

PP_Bool PPB_Instance_Proxy::SetFullscreen(PP_Instance instance,
                                          PP_Bool fullscreen) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_SetFullscreen(
      INTERFACE_ID_PPB_INSTANCE, instance, fullscreen, &result));
  return result;
}

PP_Bool PPB_Instance_Proxy::GetScreenSize(PP_Instance instance,
                                          PP_Size* size) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_GetScreenSize(
      INTERFACE_ID_PPB_INSTANCE, instance, &result, size));
  return result;
}

void PPB_Instance_Proxy::OnMsgGetWindowObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, false);
  if (enter.succeeded())
    result.Return(dispatcher(), enter.functions()->GetWindowObject(instance));
}

void PPB_Instance_Proxy::OnMsgGetOwnerElementObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, false);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->GetOwnerElementObject(instance));
  }
}

void PPB_Instance_Proxy::OnMsgBindGraphics(PP_Instance instance,
                                           HostResource device,
                                           PP_Bool* result) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, false);
  if (enter.succeeded()) {
    *result = enter.functions()->BindGraphics(instance,
                                              device.host_resource());
  }
}

void PPB_Instance_Proxy::OnMsgIsFullFrame(PP_Instance instance,
                                          PP_Bool* result) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, false);
  if (enter.succeeded())
    *result = enter.functions()->IsFullFrame(instance);
}

void PPB_Instance_Proxy::OnMsgExecuteScript(
    PP_Instance instance,
    SerializedVarReceiveInput script,
    SerializedVarOutParam out_exception,
    SerializedVarReturnValue result) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, false);
  if (enter.failed())
    return;

  if (dispatcher()->IsPlugin())
    NOTREACHED();
  else
    static_cast<HostDispatcher*>(dispatcher())->set_allow_plugin_reentrancy();

  result.Return(dispatcher(), enter.functions()->ExecuteScript(
      instance,
      script.Get(dispatcher()),
      out_exception.OutParam(dispatcher())));
}

void PPB_Instance_Proxy::OnMsgSetFullscreen(PP_Instance instance,
                                            PP_Bool fullscreen,
                                            PP_Bool* result) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, false);
  if (enter.succeeded())
    *result = enter.functions()->SetFullscreen(instance, fullscreen);
}

void PPB_Instance_Proxy::OnMsgGetScreenSize(PP_Instance instance,
                                            PP_Bool* result,
                                            PP_Size* size) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, false);
  if (enter.succeeded())
    *result = enter.functions()->GetScreenSize(instance, size);
}

}  // namespace proxy
}  // namespace pp
