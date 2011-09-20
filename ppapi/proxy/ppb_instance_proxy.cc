// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_instance_proxy.h"

#include "ppapi/c/dev/ppb_mouse_lock_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"

// Windows headers interfere with this file.
#ifdef PostMessage
#undef PostMessage
#endif

using ppapi::thunk::EnterFunctionNoLock;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Instance_FunctionAPI;

namespace ppapi {
namespace proxy {

namespace {

typedef EnterFunctionNoLock<PPB_Instance_FunctionAPI> EnterInstanceNoLock;

InterfaceProxy* CreateInstanceProxy(Dispatcher* dispatcher) {
  return new PPB_Instance_Proxy(dispatcher);
}

}  // namespace

PPB_Instance_Proxy::PPB_Instance_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_Instance_Proxy::~PPB_Instance_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Instance_Proxy::GetInfoPrivate() {
  static const Info info = {
    ppapi::thunk::GetPPB_Instance_Private_Thunk(),
    PPB_INSTANCE_PRIVATE_INTERFACE,
    INTERFACE_ID_NONE,  // 1_0 is the canonical one.
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
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_Log,
                        OnMsgLog)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_LogWithSource,
                        OnMsgLogWithSource)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_PostMessage,
                        OnMsgPostMessage)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_FlashSetFullscreen,
                        OnMsgFlashSetFullscreen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_FlashGetScreenSize,
                        OnMsgFlashGetScreenSize)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_RequestInputEvents,
                        OnMsgRequestInputEvents)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_ClearInputEvents,
                        OnMsgClearInputEvents)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_LockMouse,
                        OnMsgLockMouse)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_UnlockMouse,
                        OnMsgUnlockMouse)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

PPB_Instance_FunctionAPI* PPB_Instance_Proxy::AsPPB_Instance_FunctionAPI() {
  return this;
}

PP_Bool PPB_Instance_Proxy::BindGraphics(PP_Instance instance,
                                         PP_Resource device) {
  Resource* object = PluginResourceTracker::GetInstance()->GetResource(device);
  if (!object || object->pp_instance() != instance)
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

void PPB_Instance_Proxy::Log(PP_Instance instance,
                             int log_level,
                             PP_Var value) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_Log(
      INTERFACE_ID_PPB_INSTANCE, instance, static_cast<int>(log_level),
      SerializedVarSendInput(dispatcher(), value)));
}

void PPB_Instance_Proxy::LogWithSource(PP_Instance instance,
                                       int log_level,
                                       PP_Var source,
                                       PP_Var value) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_LogWithSource(
      INTERFACE_ID_PPB_INSTANCE, instance, static_cast<int>(log_level),
      SerializedVarSendInput(dispatcher(), source),
      SerializedVarSendInput(dispatcher(), value)));
}

void PPB_Instance_Proxy::NumberOfFindResultsChanged(PP_Instance instance,
                                                    int32_t total,
                                                    PP_Bool final_result) {
  NOTIMPLEMENTED();  // Not proxied yet.
}

void PPB_Instance_Proxy::SelectedFindResultChanged(PP_Instance instance,
                                                   int32_t index) {
  NOTIMPLEMENTED();  // Not proxied yet.
}

PP_Bool PPB_Instance_Proxy::FlashIsFullscreen(PP_Instance instance) {
  InstanceData* data = static_cast<PluginDispatcher*>(dispatcher())->
      GetInstanceData(instance);
  if (!data)
    return PP_FALSE;
  return data->fullscreen;
}

PP_Bool PPB_Instance_Proxy::FlashSetFullscreen(PP_Instance instance,
                                               PP_Bool fullscreen) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_FlashSetFullscreen(
      INTERFACE_ID_PPB_INSTANCE, instance, fullscreen, &result));
  return result;
}

PP_Bool PPB_Instance_Proxy::FlashGetScreenSize(PP_Instance instance,
                                               PP_Size* size) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_FlashGetScreenSize(
      INTERFACE_ID_PPB_INSTANCE, instance, &result, size));
  return result;
}

int32_t PPB_Instance_Proxy::RequestInputEvents(PP_Instance instance,
                                               uint32_t event_classes) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_RequestInputEvents(
      INTERFACE_ID_PPB_INSTANCE, instance, false, event_classes));

  // We always register for the classes we can handle, this function validates
  // the flags so we can notify it if anything was invalid, without requiring
  // a sync reply.
  return ValidateRequestInputEvents(false, event_classes);
}

int32_t PPB_Instance_Proxy::RequestFilteringInputEvents(
    PP_Instance instance,
    uint32_t event_classes) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_RequestInputEvents(
      INTERFACE_ID_PPB_INSTANCE, instance, true, event_classes));

  // We always register for the classes we can handle, this function validates
  // the flags so we can notify it if anything was invalid, without requiring
  // a sync reply.
  return ValidateRequestInputEvents(true, event_classes);
}

void PPB_Instance_Proxy::ClearInputEventRequest(PP_Instance instance,
                                                uint32_t event_classes) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_ClearInputEvents(
      INTERFACE_ID_PPB_INSTANCE, instance, event_classes));
}

void PPB_Instance_Proxy::ZoomChanged(PP_Instance instance,
                                     double factor) {
  // Not proxied yet.
  NOTIMPLEMENTED();
}

void PPB_Instance_Proxy::ZoomLimitsChanged(PP_Instance instance,
                                           double minimum_factor,
                                           double maximium_factor) {
  // Not proxied yet.
  NOTIMPLEMENTED();
}

void PPB_Instance_Proxy::SubscribeToPolicyUpdates(PP_Instance instance) {
  // Not proxied yet.
  NOTIMPLEMENTED();
}

void PPB_Instance_Proxy::PostMessage(PP_Instance instance,
                                     PP_Var message) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_PostMessage(
      INTERFACE_ID_PPB_INSTANCE,
      instance, SerializedVarSendInput(dispatcher(), message)));
}

int32_t PPB_Instance_Proxy::LockMouse(PP_Instance instance,
                                      PP_CompletionCallback callback) {
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;

  dispatcher()->Send(new PpapiHostMsg_PPBInstance_LockMouse(
      INTERFACE_ID_PPB_INSTANCE, instance, SendCallback(callback)));
  return PP_OK_COMPLETIONPENDING;
}

void PPB_Instance_Proxy::UnlockMouse(PP_Instance instance) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_UnlockMouse(
      INTERFACE_ID_PPB_INSTANCE, instance));
}

void PPB_Instance_Proxy::OnMsgGetWindowObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    result.Return(dispatcher(), enter.functions()->GetWindowObject(instance));
}

void PPB_Instance_Proxy::OnMsgGetOwnerElementObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->GetOwnerElementObject(instance));
  }
}

void PPB_Instance_Proxy::OnMsgBindGraphics(PP_Instance instance,
                                           const HostResource& device,
                                           PP_Bool* result) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded()) {
    *result = enter.functions()->BindGraphics(instance,
                                              device.host_resource());
  }
}

void PPB_Instance_Proxy::OnMsgIsFullFrame(PP_Instance instance,
                                          PP_Bool* result) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    *result = enter.functions()->IsFullFrame(instance);
}

void PPB_Instance_Proxy::OnMsgExecuteScript(
    PP_Instance instance,
    SerializedVarReceiveInput script,
    SerializedVarOutParam out_exception,
    SerializedVarReturnValue result) {
  EnterInstanceNoLock enter(instance, false);
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

void PPB_Instance_Proxy::OnMsgLog(PP_Instance instance,
                                  int log_level,
                                  SerializedVarReceiveInput value) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    enter.functions()->Log(instance, log_level, value.Get(dispatcher()));
}

void PPB_Instance_Proxy::OnMsgLogWithSource(PP_Instance instance,
                                            int log_level,
                                            SerializedVarReceiveInput source,
                                            SerializedVarReceiveInput value) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded()) {
    enter.functions()->LogWithSource(instance, log_level,
                                     source.Get(dispatcher()),
                                     value.Get(dispatcher()));
  }
}

void PPB_Instance_Proxy::OnMsgFlashSetFullscreen(PP_Instance instance,
                                                 PP_Bool fullscreen,
                                                 PP_Bool* result) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    *result = enter.functions()->FlashSetFullscreen(instance, fullscreen);
}

void PPB_Instance_Proxy::OnMsgFlashGetScreenSize(PP_Instance instance,
                                                 PP_Bool* result,
                                                 PP_Size* size) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    *result = enter.functions()->FlashGetScreenSize(instance, size);
}

void PPB_Instance_Proxy::OnMsgRequestInputEvents(PP_Instance instance,
                                                 bool is_filtering,
                                                 uint32_t event_classes) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded()) {
    if (is_filtering)
      enter.functions()->RequestFilteringInputEvents(instance, event_classes);
    else
      enter.functions()->RequestInputEvents(instance, event_classes);
  }
}

void PPB_Instance_Proxy::OnMsgClearInputEvents(PP_Instance instance,
                                               uint32_t event_classes) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    enter.functions()->ClearInputEventRequest(instance, event_classes);
}

void PPB_Instance_Proxy::OnMsgPostMessage(PP_Instance instance,
                                          SerializedVarReceiveInput message) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    enter.functions()->PostMessage(instance, message.Get(dispatcher()));
}

void PPB_Instance_Proxy::OnMsgLockMouse(PP_Instance instance,
                                        uint32_t serialized_callback) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return;
  PP_CompletionCallback callback = ReceiveCallback(serialized_callback);
  int32_t result = enter.functions()->LockMouse(instance, callback);
  if (result != PP_OK_COMPLETIONPENDING)
    PP_RunCompletionCallback(&callback, result);
}

void PPB_Instance_Proxy::OnMsgUnlockMouse(PP_Instance instance) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.succeeded())
    enter.functions()->UnlockMouse(instance);
}

}  // namespace proxy
}  // namespace ppapi
