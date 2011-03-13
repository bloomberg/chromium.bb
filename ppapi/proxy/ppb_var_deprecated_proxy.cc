// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_var_deprecated_proxy.h"

#include <stdlib.h>  // For malloc

#include "base/logging.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppp_class_proxy.h"
#include "ppapi/proxy/serialized_var.h"

namespace pp {
namespace proxy {

namespace {

// Used to do get the set-up information for calling a var object. If the
// exception is set, returns NULL. Otherwise, computes the dispatcher for the
// given var object. If the var is not a valid object, returns NULL and sets
// the exception.
PluginDispatcher* CheckExceptionAndGetDispatcher(const PP_Var& object,
                                                 PP_Var* exception) {
  // If an exception is already set, we don't need to do anything, just return
  // an error to the caller.
  if (exception && exception->type != PP_VARTYPE_UNDEFINED)
    return NULL;

  PluginVarTracker* tracker = PluginVarTracker::GetInstance();
  PluginDispatcher* dispatcher = tracker->DispatcherForPluginObject(object);
  if (dispatcher)
    return dispatcher;

  // The object is invalid. This means we can't figure out which dispatcher
  // to use, which is OK because the call will fail anyway. Set the exception.
  if (exception) {
    exception->type = PP_VARTYPE_STRING;
    exception->value.as_id =
        tracker->MakeString("Attempting to use an invalid object");
  }
  return NULL;
}

// PPP_Var_Deprecated plugin ---------------------------------------------------

void AddRefVar(PP_Var var) {
  PluginVarTracker::GetInstance()->AddRef(var);
}

void ReleaseVar(PP_Var var) {
  PluginVarTracker::GetInstance()->Release(var);
}

PP_Var VarFromUtf8(PP_Module module, const char* data, uint32_t len) {
  PP_Var ret;
  ret.type = PP_VARTYPE_STRING;
  ret.value.as_id = PluginVarTracker::GetInstance()->MakeString(
      data, len);
  return ret;
}

const char* VarToUtf8(PP_Var var, uint32_t* len) {
  const std::string* str =
      PluginVarTracker::GetInstance()->GetExistingString(var);
  if (str) {
    *len = static_cast<uint32_t>(str->size());
    return str->c_str();
  }
  *len = 0;
  return NULL;
}

bool HasProperty(PP_Var var,
                 PP_Var name,
                 PP_Var* exception) {
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(var, exception);
  if (!dispatcher)
    return false;

  ReceiveSerializedException se(dispatcher, exception);
  PP_Bool result = PP_FALSE;
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_HasProperty(
        INTERFACE_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name), &se, &result));
  }
  return PPBoolToBool(result);
}

bool HasMethod(PP_Var var,
               PP_Var name,
               PP_Var* exception) {
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(var, exception);
  if (!dispatcher)
    return false;

  ReceiveSerializedException se(dispatcher, exception);
  PP_Bool result = PP_FALSE;
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_HasMethodDeprecated(
        INTERFACE_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name), &se, &result));
  }
  return PPBoolToBool(result);
}

PP_Var GetProperty(PP_Var var,
                   PP_Var name,
                   PP_Var* exception) {
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(var, exception);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedException se(dispatcher, exception);
  ReceiveSerializedVarReturnValue result;
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_GetProperty(
        INTERFACE_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name), &se, &result));
  }
  return result.Return(dispatcher);
}

void EnumerateProperties(PP_Var var,
                         uint32_t* property_count,
                         PP_Var** properties,
                         PP_Var* exception) {
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(var, exception);
  if (!dispatcher) {
    *property_count = 0;
    *properties = NULL;
    return;
  }

  ReceiveSerializedVarVectorOutParam out_vector(dispatcher,
                                                property_count, properties);
  ReceiveSerializedException se(dispatcher, exception);
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_EnumerateProperties(
        INTERFACE_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, var),
        out_vector.OutParam(), &se));
  }
}

void SetProperty(PP_Var var,
                 PP_Var name,
                 PP_Var value,
                 PP_Var* exception) {
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(var, exception);
  if (!dispatcher)
    return;

  ReceiveSerializedException se(dispatcher, exception);
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_SetPropertyDeprecated(
        INTERFACE_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name),
        SerializedVarSendInput(dispatcher, value), &se));
  }
}

void RemoveProperty(PP_Var var,
                    PP_Var name,
                    PP_Var* exception) {
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(var, exception);
  if (!dispatcher)
    return;

  ReceiveSerializedException se(dispatcher, exception);
  PP_Bool result = PP_FALSE;
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_DeleteProperty(
        INTERFACE_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name), &se, &result));
  }
}

PP_Var Call(PP_Var object,
            PP_Var method_name,
            uint32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(object, exception);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  ReceiveSerializedException se(dispatcher, exception);
  if (!se.IsThrown()) {
    std::vector<SerializedVar> argv_vect;
    SerializedVarSendInput::ConvertVector(dispatcher, argv, argc, &argv_vect);

    dispatcher->Send(new PpapiHostMsg_PPBVar_CallDeprecated(
        INTERFACE_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, object),
        SerializedVarSendInput(dispatcher, method_name), argv_vect,
        &se, &result));
  }
  return result.Return(dispatcher);
}

PP_Var Construct(PP_Var object,
                 uint32_t argc,
                 PP_Var* argv,
                 PP_Var* exception) {
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(object, exception);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  ReceiveSerializedException se(dispatcher, exception);
  if (!se.IsThrown()) {
    std::vector<SerializedVar> argv_vect;
    SerializedVarSendInput::ConvertVector(dispatcher, argv, argc, &argv_vect);

    dispatcher->Send(new PpapiHostMsg_PPBVar_Construct(
        INTERFACE_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, object),
        argv_vect, &se, &result));
  }
  return result.Return(dispatcher);
}

bool IsInstanceOf(PP_Var var,
                  const PPP_Class_Deprecated* ppp_class,
                  void** ppp_class_data) {
  Dispatcher* dispatcher = CheckExceptionAndGetDispatcher(var, NULL);
  if (!dispatcher)
    return false;

  PP_Bool result = PP_FALSE;
  int64 class_int = static_cast<int64>(reinterpret_cast<intptr_t>(ppp_class));
  int64 class_data_int = 0;
  dispatcher->Send(new PpapiHostMsg_PPBVar_IsInstanceOfDeprecated(
      INTERFACE_ID_PPB_VAR_DEPRECATED, SerializedVarSendInput(dispatcher, var),
      class_int, &class_data_int, &result));
  *ppp_class_data =
      reinterpret_cast<void*>(static_cast<intptr_t>(class_data_int));
  return PPBoolToBool(result);
}

PP_Var CreateObject(PP_Instance instance,
                    const PPP_Class_Deprecated* ppp_class,
                    void* ppp_class_data) {
  Dispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_MakeUndefined();

  ReceiveSerializedVarReturnValue result;
  int64 class_int = static_cast<int64>(reinterpret_cast<intptr_t>(ppp_class));
  int64 data_int =
      static_cast<int64>(reinterpret_cast<intptr_t>(ppp_class_data));
  dispatcher->Send(new PpapiHostMsg_PPBVar_CreateObjectDeprecated(
      INTERFACE_ID_PPB_VAR_DEPRECATED, instance, class_int, data_int,
      &result));
  return result.Return(dispatcher);
}

const PPB_Var_Deprecated var_deprecated_interface = {
  &AddRefVar,
  &ReleaseVar,
  &VarFromUtf8,
  &VarToUtf8,
  &HasProperty,
  &HasMethod,
  &GetProperty,
  &EnumerateProperties,
  &SetProperty,
  &RemoveProperty,
  &Call,
  &Construct,
  &IsInstanceOf,
  &CreateObject
};

InterfaceProxy* CreateVarDeprecatedProxy(Dispatcher* dispatcher,
                                         const void* target_interface) {
  return new PPB_Var_Deprecated_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Var_Deprecated_Proxy::PPB_Var_Deprecated_Proxy(
    Dispatcher* dispatcher,
    const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Var_Deprecated_Proxy::~PPB_Var_Deprecated_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Var_Deprecated_Proxy::GetInfo() {
  static const Info info = {
    &var_deprecated_interface,
    PPB_VAR_DEPRECATED_INTERFACE,
    INTERFACE_ID_PPB_VAR_DEPRECATED,
    false,
    &CreateVarDeprecatedProxy,
  };
  return &info;
}

bool PPB_Var_Deprecated_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Var_Deprecated_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_HasProperty,
                        OnMsgHasProperty)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_HasMethodDeprecated,
                        OnMsgHasMethodDeprecated)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_GetProperty,
                        OnMsgGetProperty)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_DeleteProperty,
                        OnMsgDeleteProperty)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_EnumerateProperties,
                        OnMsgEnumerateProperties)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_SetPropertyDeprecated,
                        OnMsgSetPropertyDeprecated)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_CallDeprecated,
                        OnMsgCallDeprecated)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_Construct,
                        OnMsgConstruct)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_IsInstanceOfDeprecated,
                        OnMsgIsInstanceOfDeprecated)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_CreateObjectDeprecated,
                        OnMsgCreateObjectDeprecated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Var_Deprecated_Proxy::OnMsgHasProperty(
    SerializedVarReceiveInput var,
    SerializedVarReceiveInput name,
    SerializedVarOutParam exception,
    PP_Bool* result) {
  SetAllowPluginReentrancy();
  *result = BoolToPPBool(ppb_var_target()->HasProperty(
      var.Get(dispatcher()),
      name.Get(dispatcher()),
      exception.OutParam(dispatcher())));
}

void PPB_Var_Deprecated_Proxy::OnMsgHasMethodDeprecated(
    SerializedVarReceiveInput var,
    SerializedVarReceiveInput name,
    SerializedVarOutParam exception,
    PP_Bool* result) {
  SetAllowPluginReentrancy();
  *result = BoolToPPBool(ppb_var_target()->HasMethod(
      var.Get(dispatcher()),
      name.Get(dispatcher()),
      exception.OutParam(dispatcher())));
}

void PPB_Var_Deprecated_Proxy::OnMsgGetProperty(
    SerializedVarReceiveInput var,
    SerializedVarReceiveInput name,
    SerializedVarOutParam exception,
    SerializedVarReturnValue result) {
  SetAllowPluginReentrancy();
  result.Return(dispatcher(), ppb_var_target()->GetProperty(
      var.Get(dispatcher()), name.Get(dispatcher()),
      exception.OutParam(dispatcher())));
}

void PPB_Var_Deprecated_Proxy::OnMsgEnumerateProperties(
    SerializedVarReceiveInput var,
    SerializedVarVectorOutParam props,
    SerializedVarOutParam exception) {
  SetAllowPluginReentrancy();
  ppb_var_target()->GetAllPropertyNames(var.Get(dispatcher()),
      props.CountOutParam(), props.ArrayOutParam(dispatcher()),
      exception.OutParam(dispatcher()));
}

void PPB_Var_Deprecated_Proxy::OnMsgSetPropertyDeprecated(
    SerializedVarReceiveInput var,
    SerializedVarReceiveInput name,
    SerializedVarReceiveInput value,
    SerializedVarOutParam exception) {
  SetAllowPluginReentrancy();
  ppb_var_target()->SetProperty(var.Get(dispatcher()),
                                name.Get(dispatcher()),
                                value.Get(dispatcher()),
                                exception.OutParam(dispatcher()));
}

void PPB_Var_Deprecated_Proxy::OnMsgDeleteProperty(
    SerializedVarReceiveInput var,
    SerializedVarReceiveInput name,
    SerializedVarOutParam exception,
    PP_Bool* result) {
  SetAllowPluginReentrancy();
  ppb_var_target()->RemoveProperty(var.Get(dispatcher()),
                                   name.Get(dispatcher()),
                                   exception.OutParam(dispatcher()));
  // This deprecated function doesn't actually return a value, but we re-use
  // the message from the non-deprecated interface with the return value.
  *result = PP_TRUE;
}

void PPB_Var_Deprecated_Proxy::OnMsgCallDeprecated(
    SerializedVarReceiveInput object,
    SerializedVarReceiveInput method_name,
    SerializedVarVectorReceiveInput arg_vector,
    SerializedVarOutParam exception,
    SerializedVarReturnValue result) {
  SetAllowPluginReentrancy();
  uint32_t arg_count = 0;
  PP_Var* args = arg_vector.Get(dispatcher(), &arg_count);
  result.Return(dispatcher(), ppb_var_target()->Call(
      object.Get(dispatcher()),
      method_name.Get(dispatcher()),
      arg_count, args,
      exception.OutParam(dispatcher())));
}

void PPB_Var_Deprecated_Proxy::OnMsgConstruct(
    SerializedVarReceiveInput var,
    SerializedVarVectorReceiveInput arg_vector,
    SerializedVarOutParam exception,
    SerializedVarReturnValue result) {
  SetAllowPluginReentrancy();
  uint32_t arg_count = 0;
  PP_Var* args = arg_vector.Get(dispatcher(), &arg_count);
  result.Return(dispatcher(), ppb_var_target()->Construct(
      var.Get(dispatcher()), arg_count, args,
      exception.OutParam(dispatcher())));
}

void PPB_Var_Deprecated_Proxy::OnMsgIsInstanceOfDeprecated(
    pp::proxy::SerializedVarReceiveInput var,
    int64 ppp_class,
    int64* ppp_class_data,
    PP_Bool* result) {
  // TODO(brettw) write this.
}

void PPB_Var_Deprecated_Proxy::OnMsgCreateObjectDeprecated(
    PP_Instance instance,
    int64 ppp_class,
    int64 class_data,
    SerializedVarReturnValue result) {
  SetAllowPluginReentrancy();
  result.Return(dispatcher(), PPP_Class_Proxy::CreateProxiedObject(
      ppb_var_target(), dispatcher(), instance, ppp_class, class_data));
}

void PPB_Var_Deprecated_Proxy::SetAllowPluginReentrancy() {
  if (dispatcher()->IsPlugin())
    NOTREACHED();
  else
    static_cast<HostDispatcher*>(dispatcher())->set_allow_plugin_reentrancy();
}

}  // namespace proxy
}  // namespace pp
