// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_instance_proxy.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_mouse_lock.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppb_url_util_shared.h"
#include "ppapi/shared_impl/ppb_view_shared.h"
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
    : InterfaceProxy(dispatcher),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Instance_Proxy::~PPB_Instance_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Instance_Proxy::GetInfoPrivate() {
  static const Info info = {
    ppapi::thunk::GetPPB_Instance_Private_0_1_Thunk(),
    PPB_INSTANCE_PRIVATE_INTERFACE_0_1,
    API_ID_NONE,  // 1_0 is the canonical one.
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
    // Plugin -> Host messages.
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetWindowObject,
                        OnHostMsgGetWindowObject)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetOwnerElementObject,
                        OnHostMsgGetOwnerElementObject)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_BindGraphics,
                        OnHostMsgBindGraphics)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_IsFullFrame,
                        OnHostMsgIsFullFrame)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_ExecuteScript,
                        OnHostMsgExecuteScript)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetDefaultCharSet,
                        OnHostMsgGetDefaultCharSet)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_Log,
                        OnHostMsgLog)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_LogWithSource,
                        OnHostMsgLogWithSource)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_PostMessage,
                        OnHostMsgPostMessage)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_FlashSetFullscreen,
                        OnHostMsgFlashSetFullscreen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_FlashGetScreenSize,
                        OnHostMsgFlashGetScreenSize)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_SetFullscreen,
                        OnHostMsgSetFullscreen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetScreenSize,
                        OnHostMsgGetScreenSize)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_RequestInputEvents,
                        OnHostMsgRequestInputEvents)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_ClearInputEvents,
                        OnHostMsgClearInputEvents)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_LockMouse,
                        OnHostMsgLockMouse)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_UnlockMouse,
                        OnHostMsgUnlockMouse)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_ResolveRelativeToDocument,
                        OnHostMsgResolveRelativeToDocument)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_DocumentCanRequest,
                        OnHostMsgDocumentCanRequest)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_DocumentCanAccessDocument,
                        OnHostMsgDocumentCanAccessDocument)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetDocumentURL,
                        OnHostMsgGetDocumentURL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBInstance_GetPluginInstanceURL,
                        OnHostMsgGetPluginInstanceURL)

    // Host -> Plugin messages.
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBInstance_MouseLockComplete,
                        OnPluginMsgMouseLockComplete)

    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

PPB_Instance_FunctionAPI* PPB_Instance_Proxy::AsPPB_Instance_FunctionAPI() {
  return this;
}

PP_Bool PPB_Instance_Proxy::BindGraphics(PP_Instance instance,
                                         PP_Resource device) {
  Resource* object =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(device);
  if (!object || object->pp_instance() != instance)
    return PP_FALSE;

  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_BindGraphics(
      API_ID_PPB_INSTANCE, instance, object->host_resource(),
      &result));
  return result;
}

PP_Bool PPB_Instance_Proxy::IsFullFrame(PP_Instance instance) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_IsFullFrame(
      API_ID_PPB_INSTANCE, instance, &result));
  return result;
}

const ViewData* PPB_Instance_Proxy::GetViewData(PP_Instance instance) {
  InstanceData* data = static_cast<PluginDispatcher*>(dispatcher())->
      GetInstanceData(instance);
  if (!data)
    return NULL;
  return &data->view;
}

PP_Var PPB_Instance_Proxy::GetWindowObject(PP_Instance instance) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_GetWindowObject(
      API_ID_PPB_INSTANCE, instance, &result));
  return result.Return(dispatcher());
}

PP_Var PPB_Instance_Proxy::GetOwnerElementObject(PP_Instance instance) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_GetOwnerElementObject(
      API_ID_PPB_INSTANCE, instance, &result));
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
      API_ID_PPB_INSTANCE, instance,
      SerializedVarSendInput(dispatcher(), script), &se, &result));
  return result.Return(dispatcher());
}

PP_Var PPB_Instance_Proxy::GetDefaultCharSet(PP_Instance instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  dispatcher->Send(new PpapiHostMsg_PPBInstance_GetDefaultCharSet(
      API_ID_PPB_INSTANCE, instance, &result));
  return result.Return(dispatcher);
}

void PPB_Instance_Proxy::Log(PP_Instance instance,
                             int log_level,
                             PP_Var value) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_Log(
      API_ID_PPB_INSTANCE, instance, static_cast<int>(log_level),
      SerializedVarSendInput(dispatcher(), value)));
}

void PPB_Instance_Proxy::LogWithSource(PP_Instance instance,
                                       int log_level,
                                       PP_Var source,
                                       PP_Var value) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_LogWithSource(
      API_ID_PPB_INSTANCE, instance, static_cast<int>(log_level),
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
  return data->flash_fullscreen;
}

PP_Bool PPB_Instance_Proxy::SetFullscreen(PP_Instance instance,
                                          PP_Bool fullscreen) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_SetFullscreen(
      API_ID_PPB_INSTANCE, instance, fullscreen, &result));
  return result;
}

PP_Bool PPB_Instance_Proxy::FlashSetFullscreen(PP_Instance instance,
                                               PP_Bool fullscreen) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_FlashSetFullscreen(
      API_ID_PPB_INSTANCE, instance, fullscreen, &result));
  return result;
}

PP_Bool PPB_Instance_Proxy::GetScreenSize(PP_Instance instance,
                                          PP_Size* size) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_GetScreenSize(
      API_ID_PPB_INSTANCE, instance, &result, size));
  return result;
}

PP_Bool PPB_Instance_Proxy::FlashGetScreenSize(PP_Instance instance,
                                               PP_Size* size) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_FlashGetScreenSize(
      API_ID_PPB_INSTANCE, instance, &result, size));
  return result;
}

void PPB_Instance_Proxy::SampleGamepads(PP_Instance instance,
                                        PP_GamepadsData_Dev* data) {
  NOTIMPLEMENTED();
}

int32_t PPB_Instance_Proxy::RequestInputEvents(PP_Instance instance,
                                               uint32_t event_classes) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_RequestInputEvents(
      API_ID_PPB_INSTANCE, instance, false, event_classes));

  // We always register for the classes we can handle, this function validates
  // the flags so we can notify it if anything was invalid, without requiring
  // a sync reply.
  return ValidateRequestInputEvents(false, event_classes);
}

int32_t PPB_Instance_Proxy::RequestFilteringInputEvents(
    PP_Instance instance,
    uint32_t event_classes) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_RequestInputEvents(
      API_ID_PPB_INSTANCE, instance, true, event_classes));

  // We always register for the classes we can handle, this function validates
  // the flags so we can notify it if anything was invalid, without requiring
  // a sync reply.
  return ValidateRequestInputEvents(true, event_classes);
}

void PPB_Instance_Proxy::ClearInputEventRequest(PP_Instance instance,
                                                uint32_t event_classes) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_ClearInputEvents(
      API_ID_PPB_INSTANCE, instance, event_classes));
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

PP_Var PPB_Instance_Proxy::ResolveRelativeToDocument(
    PP_Instance instance,
    PP_Var relative,
    PP_URLComponents_Dev* components) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_ResolveRelativeToDocument(
      API_ID_PPB_INSTANCE, instance,
      SerializedVarSendInput(dispatcher(), relative),
      &result));
  return PPB_URLUtil_Shared::ConvertComponentsAndReturnURL(
      result.Return(dispatcher()),
      components);
}

PP_Bool PPB_Instance_Proxy::DocumentCanRequest(PP_Instance instance,
                                               PP_Var url) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_DocumentCanRequest(
      API_ID_PPB_INSTANCE, instance,
      SerializedVarSendInput(dispatcher(), url),
      &result));
  return result;
}

PP_Bool PPB_Instance_Proxy::DocumentCanAccessDocument(PP_Instance instance,
                                                      PP_Instance target) {
  PP_Bool result = PP_FALSE;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_DocumentCanAccessDocument(
      API_ID_PPB_INSTANCE, instance, target, &result));
  return result;
}

PP_Var PPB_Instance_Proxy::GetDocumentURL(PP_Instance instance,
                                          PP_URLComponents_Dev* components) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_GetDocumentURL(
      API_ID_PPB_INSTANCE, instance, &result));
  return PPB_URLUtil_Shared::ConvertComponentsAndReturnURL(
      result.Return(dispatcher()),
      components);
}

PP_Var PPB_Instance_Proxy::GetPluginInstanceURL(
      PP_Instance instance,
      PP_URLComponents_Dev* components) {
  ReceiveSerializedVarReturnValue result;
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_GetPluginInstanceURL(
      API_ID_PPB_INSTANCE, instance, &result));
  return PPB_URLUtil_Shared::ConvertComponentsAndReturnURL(
      result.Return(dispatcher()),
      components);
}

void PPB_Instance_Proxy::PostMessage(PP_Instance instance,
                                     PP_Var message) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_PostMessage(
      API_ID_PPB_INSTANCE,
      instance, SerializedVarSendInput(dispatcher(), message)));
}

int32_t PPB_Instance_Proxy::LockMouse(PP_Instance instance,
                                      PP_CompletionCallback callback) {
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;

  // Save the mouse callback on the instance data.
  InstanceData* data = static_cast<PluginDispatcher*>(dispatcher())->
      GetInstanceData(instance);
  if (!data)
    return PP_ERROR_BADARGUMENT;
  if (data->mouse_lock_callback.func)
    return PP_ERROR_INPROGRESS;  // Already have a pending callback.
  data->mouse_lock_callback = callback;

  dispatcher()->Send(new PpapiHostMsg_PPBInstance_LockMouse(
      API_ID_PPB_INSTANCE, instance));
  return PP_OK_COMPLETIONPENDING;
}

void PPB_Instance_Proxy::UnlockMouse(PP_Instance instance) {
  dispatcher()->Send(new PpapiHostMsg_PPBInstance_UnlockMouse(
      API_ID_PPB_INSTANCE, instance));
}

void PPB_Instance_Proxy::OnHostMsgGetWindowObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    result.Return(dispatcher(), enter.functions()->GetWindowObject(instance));
}

void PPB_Instance_Proxy::OnHostMsgGetOwnerElementObject(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->GetOwnerElementObject(instance));
  }
}

void PPB_Instance_Proxy::OnHostMsgBindGraphics(PP_Instance instance,
                                               const HostResource& device,
                                               PP_Bool* result) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded()) {
    *result = enter.functions()->BindGraphics(instance,
                                              device.host_resource());
  }
}

void PPB_Instance_Proxy::OnHostMsgIsFullFrame(PP_Instance instance,
                                              PP_Bool* result) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    *result = enter.functions()->IsFullFrame(instance);
}

void PPB_Instance_Proxy::OnHostMsgExecuteScript(
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

void PPB_Instance_Proxy::OnHostMsgGetDefaultCharSet(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    result.Return(dispatcher(), enter.functions()->GetDefaultCharSet(instance));
}

void PPB_Instance_Proxy::OnHostMsgLog(PP_Instance instance,
                                      int log_level,
                                      SerializedVarReceiveInput value) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    enter.functions()->Log(instance, log_level, value.Get(dispatcher()));
}

void PPB_Instance_Proxy::OnHostMsgLogWithSource(
    PP_Instance instance,
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

void PPB_Instance_Proxy::OnHostMsgSetFullscreen(PP_Instance instance,
                                                PP_Bool fullscreen,
                                                PP_Bool* result) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    *result = enter.functions()->SetFullscreen(instance, fullscreen);
}


void PPB_Instance_Proxy::OnHostMsgFlashSetFullscreen(PP_Instance instance,
                                                     PP_Bool fullscreen,
                                                     PP_Bool* result) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, false);
  if (enter.succeeded())
    *result = enter.functions()->FlashSetFullscreen(instance, fullscreen);
}

void PPB_Instance_Proxy::OnHostMsgGetScreenSize(PP_Instance instance,
                                                PP_Bool* result,
                                                PP_Size* size) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    *result = enter.functions()->GetScreenSize(instance, size);
}

void PPB_Instance_Proxy::OnHostMsgFlashGetScreenSize(PP_Instance instance,
                                                     PP_Bool* result,
                                                     PP_Size* size) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, false);
  if (enter.succeeded())
    *result = enter.functions()->FlashGetScreenSize(instance, size);
}

void PPB_Instance_Proxy::OnHostMsgRequestInputEvents(PP_Instance instance,
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

void PPB_Instance_Proxy::OnHostMsgClearInputEvents(PP_Instance instance,
                                                   uint32_t event_classes) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    enter.functions()->ClearInputEventRequest(instance, event_classes);
}

void PPB_Instance_Proxy::OnHostMsgPostMessage(PP_Instance instance,
                                          SerializedVarReceiveInput message) {
  EnterInstanceNoLock enter(instance, false);
  if (enter.succeeded())
    enter.functions()->PostMessage(instance, message.Get(dispatcher()));
}

void PPB_Instance_Proxy::OnHostMsgLockMouse(PP_Instance instance) {
  EnterHostFunctionForceCallback<PPB_Instance_FunctionAPI> enter(
      instance, callback_factory_,
      &PPB_Instance_Proxy::MouseLockCompleteInHost, instance);
  if (enter.succeeded())
    enter.SetResult(enter.functions()->LockMouse(instance, enter.callback()));
}

void PPB_Instance_Proxy::OnHostMsgUnlockMouse(PP_Instance instance) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.succeeded())
    enter.functions()->UnlockMouse(instance);
}

void PPB_Instance_Proxy::OnHostMsgResolveRelativeToDocument(
    PP_Instance instance,
    SerializedVarReceiveInput relative,
    SerializedVarReturnValue result) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->ResolveRelativeToDocument(
                      instance, relative.Get(dispatcher()), NULL));
  }
}

void PPB_Instance_Proxy::OnHostMsgDocumentCanRequest(
    PP_Instance instance,
    SerializedVarReceiveInput url,
    PP_Bool* result) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.succeeded()) {
    *result = enter.functions()->DocumentCanRequest(instance,
                                                    url.Get(dispatcher()));
  }
}

void PPB_Instance_Proxy::OnHostMsgDocumentCanAccessDocument(PP_Instance active,
                                                            PP_Instance target,
                                                            PP_Bool* result) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(active, true);
  if (enter.succeeded())
    *result = enter.functions()->DocumentCanAccessDocument(active, target);
}

void PPB_Instance_Proxy::OnHostMsgGetDocumentURL(PP_Instance instance,
                                                 SerializedVarReturnValue result) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->GetDocumentURL(instance, NULL));
  }
}

void PPB_Instance_Proxy::OnHostMsgGetPluginInstanceURL(
    PP_Instance instance,
    SerializedVarReturnValue result) {
  EnterFunctionNoLock<PPB_Instance_FunctionAPI> enter(instance, true);
  if (enter.succeeded()) {
    result.Return(dispatcher(),
                  enter.functions()->GetPluginInstanceURL(instance, NULL));
  }
}

void PPB_Instance_Proxy::OnPluginMsgMouseLockComplete(PP_Instance instance,
                                                      int32_t result) {
  // Save the mouse callback on the instance data.
  InstanceData* data = static_cast<PluginDispatcher*>(dispatcher())->
      GetInstanceData(instance);
  if (!data)
    return;  // Instance was probably deleted.
  if (!data->mouse_lock_callback.func) {
    NOTREACHED();
    return;
  }
  PP_RunAndClearCompletionCallback(&data->mouse_lock_callback, result);
}

void PPB_Instance_Proxy::MouseLockCompleteInHost(int32_t result,
                                                 PP_Instance instance) {
  dispatcher()->Send(new PpapiMsg_PPBInstance_MouseLockComplete(
      API_ID_PPB_INSTANCE, instance, result));
}

}  // namespace proxy
}  // namespace ppapi
