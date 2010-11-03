// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_var_proxy.h"

#include <stdlib.h>  // For malloc

#include "base/logging.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/proxy/dispatcher.h"
#include "ppapi/proxy/ppapi_message_helpers.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"

namespace pp {
namespace proxy {

namespace {

void AddRefVar(PP_Var var) {
  Dispatcher::Get()->plugin_var_tracker()->AddRef(var);
}

void ReleaseVar(PP_Var var) {
  Dispatcher::Get()->plugin_var_tracker()->Release(var);
}

PP_Var VarFromUtf8(PP_Module module_id, const char* data, uint32_t len) {
  PP_Var ret;
  ret.type = PP_VARTYPE_STRING;
  // TODO(brettw) avoid this extra copy.
  ret.value.as_id = Dispatcher::Get()->plugin_var_tracker()->MakeString(
      std::string(data, len));
  return ret;
}

const char* VarToUtf8(PP_Var var, uint32_t* len) {
  const std::string* str =
      Dispatcher::Get()->plugin_var_tracker()->GetExistingString(var);
  if (str) {
    *len = static_cast<uint32_t>(str->size());
    return str->c_str();
  } else {
    *len = 0;
    return NULL;
  }
}

PP_Var ConvertType(PP_Instance instance,
                   PP_Var var,
                   PP_VarType new_type,
                   PP_Var* exception) {
  Dispatcher* dispatcher = Dispatcher::Get();
  ReceiveSerializedException se(dispatcher, exception);
  ReceiveSerializedVarReturnValue result(dispatcher);
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_ConvertType(
        INTERFACE_ID_PPB_VAR_DEPRECATED, instance,
        SerializedVarSendInput(dispatcher, var),
        static_cast<int>(new_type), &se, &result));
  }
  return result.Return();
}

bool HasProperty(PP_Var var,
                 PP_Var name,
                 PP_Var* exception) {
  Dispatcher* dispatcher = Dispatcher::Get();
  ReceiveSerializedException se(dispatcher, exception);
  bool result = false;
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_HasProperty(
        INTERFACE_ID_PPB_VAR_DEPRECATED, SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name), &se, &result));
  }
  return result;
}

bool HasMethodDeprecated(PP_Var var,
                         PP_Var name,
                         PP_Var* exception) {
  Dispatcher* dispatcher = Dispatcher::Get();
  ReceiveSerializedException se(dispatcher, exception);
  bool result = false;
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_HasMethodDeprecated(
        INTERFACE_ID_PPB_VAR_DEPRECATED, SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name), &se, &result));
  }
  return result;
}

PP_Var GetProperty(PP_Var var,
                   PP_Var name,
                   PP_Var* exception) {
  Dispatcher* dispatcher = Dispatcher::Get();
  ReceiveSerializedException se(dispatcher, exception);
  ReceiveSerializedVarReturnValue result(dispatcher);
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_GetProperty(
        INTERFACE_ID_PPB_VAR_DEPRECATED, SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name), &se, &result));
  }
  return result.Return();
}

void EnumerateProperties(PP_Var var,
                         uint32_t* property_count,
                         PP_Var** properties,
                         PP_Var* exception) {
  Dispatcher* dispatcher = Dispatcher::Get();

  std::vector<SerializedVar> props;
  ReceiveSerializedException se(dispatcher, exception);
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_EnumerateProperties(
        INTERFACE_ID_PPB_VAR_DEPRECATED, SerializedVarSendInput(dispatcher, var),
        &props, &se));
  }

  // TODO(brettw) factor this out so it can be used by ScriptableObject.
  /*
  *property_count = static_cast<uint32_t>(props.size());
  if (property_count) {
    *properties = static_cast<PP_Var*>(
        malloc(*property_count * sizeof(PP_Var*)));
    for (size_t i = 0; i < props.size(); i++) {
      ReceiveSerializedVarReturnValue converted(dispatcher);
      SerializedVar* converted_sv = &convered;

      *static_cast<SerializedVar*>(&converted) = props[i];
      (*properties)[i] = converted.Return();
    }
  } else {
    *properties = NULL;
  }
  */
}

void SetPropertyDeprecated(PP_Var var,
                           PP_Var name,
                           PP_Var value,
                           PP_Var* exception) {
  Dispatcher* dispatcher = Dispatcher::Get();
  ReceiveSerializedException se(dispatcher, exception);
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_SetPropertyDeprecated(
        INTERFACE_ID_PPB_VAR_DEPRECATED, SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name),
        SerializedVarSendInput(dispatcher, value), &se));
  }
}

bool DeleteProperty(PP_Var var,
                    PP_Var name,
                    PP_Var* exception) {
  Dispatcher* dispatcher = Dispatcher::Get();
  ReceiveSerializedException se(dispatcher, exception);
  bool result = false;
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_DeleteProperty(
        INTERFACE_ID_PPB_VAR_DEPRECATED, SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name), &se, &result));
  }
  return result;
}

void RemovePropertyDeprecated(PP_Var var,
                              PP_Var name,
                              PP_Var* exception) {
  Dispatcher* dispatcher = Dispatcher::Get();
  ReceiveSerializedException se(dispatcher, exception);
  bool result = false;
  if (!se.IsThrown()) {
    dispatcher->Send(new PpapiHostMsg_PPBVar_DeleteProperty(
        INTERFACE_ID_PPB_VAR_DEPRECATED, SerializedVarSendInput(dispatcher, var),
        SerializedVarSendInput(dispatcher, name), &se, &result));
  }
}


PP_Var Call(PP_Var object,
            PP_Var this_object,
            PP_Var method_name,
            uint32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  Dispatcher* dispatcher = Dispatcher::Get();
  ReceiveSerializedVarReturnValue result(dispatcher);
  ReceiveSerializedException se(dispatcher, exception);
  if (!se.IsThrown()) {
    std::vector<SerializedVar> argv_vect;
    SerializedVarSendInput::ConvertVector(dispatcher, argv, argc, &argv_vect);

    dispatcher->Send(new PpapiHostMsg_PPBVar_Call(
        INTERFACE_ID_PPB_VAR_DEPRECATED,
        SerializedVarSendInput(dispatcher, object),
        SerializedVarSendInput(dispatcher, this_object),
        SerializedVarSendInput(dispatcher, method_name), argv_vect,
        &se, &result));
  }
  return result.Return();
}

PP_Var CallDeprecated(PP_Var object,
                      PP_Var method_name,
                      uint32_t argc,
                      PP_Var* argv,
                      PP_Var* exception) {
  Dispatcher* dispatcher = Dispatcher::Get();
  ReceiveSerializedVarReturnValue result(dispatcher);
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
  return result.Return();
}

PP_Var Construct(PP_Var object,
                 uint32_t argc,
                 PP_Var* argv,
                 PP_Var* exception) {
  Dispatcher* dispatcher = Dispatcher::Get();
  ReceiveSerializedVarReturnValue result(dispatcher);
  ReceiveSerializedException se(dispatcher, exception);
  if (!se.IsThrown()) {
    std::vector<SerializedVar> argv_vect;
    SerializedVarSendInput::ConvertVector(dispatcher, argv, argc, &argv_vect);

    dispatcher->Send(new PpapiHostMsg_PPBVar_Construct(
        INTERFACE_ID_PPB_VAR_DEPRECATED, SerializedVarSendInput(dispatcher, object),
        argv_vect, &se, &result));
  }
  return result.Return();
}

bool IsInstanceOfDeprecated(PP_Var var,
                            const PPP_Class_Deprecated* ppp_class,
                            void** ppp_class_data) {
  bool result = false;
  Dispatcher* dispatcher = Dispatcher::Get();
  int64 class_int = static_cast<int64>(reinterpret_cast<intptr_t>(ppp_class));
  int64 class_data_int = 0;
  dispatcher->Send(new PpapiHostMsg_PPBVar_IsInstanceOfDeprecated(
      INTERFACE_ID_PPB_VAR_DEPRECATED, SerializedVarSendInput(dispatcher, var),
      class_int, &class_data_int, &result));
  *ppp_class_data =
      reinterpret_cast<void*>(static_cast<intptr_t>(class_data_int));
  return result;
}

PP_Var CreateObjectDeprecated(PP_Module module_id,
                              const PPP_Class_Deprecated* ppp_class,
                              void* ppp_class_data) {
  Dispatcher* dispatcher = Dispatcher::Get();
  ReceiveSerializedVarReturnValue result(dispatcher);
  int64 class_int = static_cast<int64>(reinterpret_cast<intptr_t>(ppp_class));
  int64 data_int =
      static_cast<int64>(reinterpret_cast<intptr_t>(ppp_class_data));
  dispatcher->Send(new PpapiHostMsg_PPBVar_CreateObjectDeprecated(
      INTERFACE_ID_PPB_VAR_DEPRECATED, module_id, class_int, data_int, &result));
  return result.Return();
}

const PPB_Var var_interface = {
  &AddRefVar,
  &ReleaseVar,
  &VarFromUtf8,
  &VarToUtf8,
  &ConvertType,
};


const PPB_Var_Deprecated var_deprecated_interface = {
  &AddRefVar,
  &ReleaseVar,
  &VarFromUtf8,
  &VarToUtf8,
  &HasProperty,
  &HasMethodDeprecated,
  &GetProperty,
  &EnumerateProperties,
  &SetPropertyDeprecated,
  &RemovePropertyDeprecated,
  &CallDeprecated,
  &Construct,
  &IsInstanceOfDeprecated,
  &CreateObjectDeprecated
};

}  // namespace

PPB_Var_Proxy::PPB_Var_Proxy(Dispatcher* dispatcher,
                               const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Var_Proxy::~PPB_Var_Proxy() {
}

const void* PPB_Var_Proxy::GetSourceInterface() const {
  return &var_deprecated_interface;
}

InterfaceID PPB_Var_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_VAR_DEPRECATED;
}

void PPB_Var_Proxy::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PPB_Var_Proxy, msg)
    // AddRef /Release here
    //IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_DefineProperty,
    //                    OnMsgDefineProperty)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_ConvertType,
                        OnMsgConvertType)
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
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_IsCallable,
                        OnMsgIsCallable)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_Call,
                        OnMsgCall)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_CallDeprecated,
                        OnMsgCallDeprecated)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_Construct,
                        OnMsgConstruct)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_IsInstanceOfDeprecated,
                        OnMsgIsInstanceOfDeprecated)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBVar_CreateObjectDeprecated,
                        OnMsgCreateObjectDeprecated)
  IPC_END_MESSAGE_MAP()
  // FIXME(brettw) handle bad messages!
}

void PPB_Var_Proxy::OnMsgConvertType(PP_Instance instance,
                                     SerializedVarReceiveInput var,
                                     int new_type,
                                     SerializedVarOutParam exception,
                                     SerializedVarReturnValue result) {
  // FIXME(brettw) write this.
}

void PPB_Var_Proxy::OnMsgHasProperty(SerializedVarReceiveInput var,
                                     SerializedVarReceiveInput name,
                                     SerializedVarOutParam exception,
                                     bool* result) {
  *result = ppb_var_target()->HasProperty(var.Get(dispatcher()),
                                          name.Get(dispatcher()),
                                          exception.OutParam(dispatcher()));
}

void PPB_Var_Proxy::OnMsgHasMethodDeprecated(SerializedVarReceiveInput var,
                                             SerializedVarReceiveInput name,
                                             SerializedVarOutParam exception,
                                             bool* result) {
  *result = ppb_var_target()->HasMethod(var.Get(dispatcher()),
                                        name.Get(dispatcher()),
                                        exception.OutParam(dispatcher()));
}

void PPB_Var_Proxy::OnMsgGetProperty(SerializedVarReceiveInput var,
                                     SerializedVarReceiveInput name,
                                     SerializedVarOutParam exception,
                                     SerializedVarReturnValue result) {
  result.Return(dispatcher(), ppb_var_target()->GetProperty(
      var.Get(dispatcher()), name.Get(dispatcher()),
      exception.OutParam(dispatcher())));
}

void PPB_Var_Proxy::OnMsgEnumerateProperties(
    SerializedVarReceiveInput var,
    std::vector<pp::proxy::SerializedVar>* props,
    SerializedVarOutParam exception) {
  // FIXME(brettw) write this.
}

void PPB_Var_Proxy::OnMsgSetPropertyDeprecated(
    SerializedVarReceiveInput var,
    SerializedVarReceiveInput name,
    SerializedVarReceiveInput value,
    SerializedVarOutParam exception) {
  ppb_var_target()->SetProperty(var.Get(dispatcher()),
                                name.Get(dispatcher()),
                                value.Get(dispatcher()),
                                exception.OutParam(dispatcher()));
}

void PPB_Var_Proxy::OnMsgIsCallable(SerializedVarReceiveInput object,
                                    bool* result) {
  // FIXME(brettw) write this.
}

void PPB_Var_Proxy::OnMsgDeleteProperty(SerializedVarReceiveInput var,
                                        SerializedVarReceiveInput name,
                                        SerializedVarOutParam exception,
                                        bool* result) {
  // FIXME(brettw) write this.
}

void PPB_Var_Proxy::OnMsgCall(
    SerializedVarReceiveInput object,
    SerializedVarReceiveInput this_object,
    SerializedVarReceiveInput method_name,
    SerializedVarVectorReceiveInput arg_vector,
    SerializedVarOutParam exception,
    SerializedVarReturnValue result) {
  // FIXME(brettw) write this.
}

void PPB_Var_Proxy::OnMsgCallDeprecated(
    SerializedVarReceiveInput object,
    SerializedVarReceiveInput method_name,
    SerializedVarVectorReceiveInput arg_vector,
    SerializedVarOutParam exception,
    SerializedVarReturnValue result) {
  uint32_t arg_count = 0;
  PP_Var* args = arg_vector.Get(dispatcher(), &arg_count);
  result.Return(dispatcher(), ppb_var_target()->Call(
      object.Get(dispatcher()),
      method_name.Get(dispatcher()),
      arg_count, args,
      exception.OutParam(dispatcher())));
}

void PPB_Var_Proxy::OnMsgConstruct(
    SerializedVarReceiveInput var,
    SerializedVarVectorReceiveInput arg_vector,
    SerializedVarOutParam exception,
    SerializedVarReturnValue result) {
  uint32_t arg_count = 0;
  PP_Var* args = arg_vector.Get(dispatcher(), &arg_count);
  result.Return(dispatcher(), ppb_var_target()->Construct(
      var.Get(dispatcher()), arg_count, args,
      exception.OutParam(dispatcher())));
}

void PPB_Var_Proxy::OnMsgIsInstanceOfDeprecated(
    pp::proxy::SerializedVarReceiveInput var,
    int64 ppp_class,
    int64* ppp_class_data,
    bool* result) {
  // TODO(brettw) write this.
}

void PPB_Var_Proxy::OnMsgCreateObjectDeprecated(
    PP_Module module_id,
    int64 ppp_class,
    int64 ppp_class_data,
    SerializedVarReturnValue result) {
  result.Return(dispatcher(), ppb_var_target()->CreateObject(
      module_id,
      reinterpret_cast<const PPP_Class_Deprecated*>(
          static_cast<intptr_t>(ppp_class)),
      reinterpret_cast<void*>(static_cast<intptr_t>(ppp_class_data))));
}

}  // namespace proxy
}  // namespace pp
