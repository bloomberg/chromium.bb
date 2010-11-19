/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/object_proxy.h"

#include <map>
#include <string>

#include "srpcgen/ppb_rpc.h"
#include "srpcgen/ppp_rpc.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability_process.h"
#ifdef __native_client__
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#else
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#endif  // __native_client__
#include "native_client/src/shared/ppapi_proxy/object_capability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/dev/ppp_class_deprecated.h"
#include "ppapi/c/pp_var.h"

namespace ppapi_proxy {

namespace {

std::map<ObjectCapability, PP_Var*>* capability_proxy_map = NULL;

const uint32_t kMaxVarSize = 64 * 1024;

}  // namespace

bool ObjectProxy::HasProperty(PP_Var name,
                              PP_Var* exception) {
  DebugPrintf("ObjectProxy::HasProperty\n");
  uint32_t name_length = kMaxVarSize;
  nacl::scoped_array<char> name_chars(Serialize(&name, 1, &name_length));
  if (name_chars == NULL) {
    return false;
  }
  uint32_t ex_in_length = kMaxVarSize;
  nacl::scoped_array<char> ex_in_chars(Serialize(exception, 1, &ex_in_length));
  if (ex_in_chars == NULL) {
    return false;
  }
  uint32_t ex_length = kMaxVarSize;
  nacl::scoped_array<char> ex_chars(new char[kMaxVarSize]);
  int32_t success;
  NaClSrpcError retval =
      ObjectStubRpcClient::HasProperty(
          channel_,
          sizeof(ObjectCapability),
          reinterpret_cast<char*>(&capability_),
          name_length,
          name_chars.get(),
          ex_in_length,
          ex_in_chars.get(),
          &success,
          &ex_length,
          ex_chars.get());
  if (retval != NACL_SRPC_RESULT_OK) {
    return false;
  }
  if (exception != NULL) {
    (void) DeserializeTo(channel_, ex_chars.get(), ex_length, 1, exception);
  }
  return success ? true : false;
}


bool ObjectProxy::HasMethod(PP_Var name,
                            PP_Var* exception) {
  DebugPrintf("ObjectProxy::HasMethod\n");
  uint32_t name_length = kMaxVarSize;
  nacl::scoped_array<char> name_chars(Serialize(&name, 1, &name_length));
  if (name_chars == NULL) {
    return false;
  }
  uint32_t ex_in_length = kMaxVarSize;
  nacl::scoped_array<char> ex_in_chars(Serialize(exception, 1, &ex_in_length));
  if (ex_in_chars == NULL) {
    return false;
  }
  uint32_t ex_length = kMaxVarSize;
  nacl::scoped_array<char> ex_chars(new char[kMaxVarSize]);
  int32_t success;
  NaClSrpcError retval =
      ObjectStubRpcClient::HasMethod(
          channel_,
          sizeof(ObjectCapability),
          reinterpret_cast<char*>(&capability_),
          name_length,
          name_chars.get(),
          ex_in_length,
          ex_in_chars.get(),
          &success,
          &ex_length,
          ex_chars.get());
  if (retval != NACL_SRPC_RESULT_OK) {
    return false;
  }
  if (exception != NULL) {
    (void) DeserializeTo(channel_, ex_chars.get(), ex_length, 1, exception);
  }
  return success ? true : false;
}


PP_Var ObjectProxy::GetProperty(PP_Var name,
                                PP_Var* exception) {
  DebugPrintf("ObjectProxy::GetProperty\n");
  PP_Var value = PP_MakeUndefined();
  uint32_t name_length = kMaxVarSize;
  nacl::scoped_array<char> name_chars(Serialize(&name, 1, &name_length));
  if (name_chars == NULL) {
    return value;
  }
  uint32_t ex_in_length = kMaxVarSize;
  nacl::scoped_array<char> ex_in_chars(Serialize(exception, 1, &ex_in_length));
  if (ex_in_chars == NULL) {
    return value;
  }
  uint32_t value_length = kMaxVarSize;
  nacl::scoped_array<char> value_chars(new char[kMaxVarSize]);
  uint32_t ex_length = kMaxVarSize;
  nacl::scoped_array<char> ex_chars(new char[kMaxVarSize]);
  NaClSrpcError retval =
      ObjectStubRpcClient::GetProperty(
          channel_,
          sizeof(ObjectCapability),
          reinterpret_cast<char*>(&capability_),
          name_length,
          name_chars.get(),
          ex_in_length,
          ex_in_chars.get(),
          &value_length,
          value_chars.get(),
          &ex_length,
          ex_chars.get());
  if (retval != NACL_SRPC_RESULT_OK) {
    return value;
  }
  if (!DeserializeTo(channel_, value_chars.get(), value_length, 1, &value)) {
    return value;
  }
  if (exception != NULL) {
    (void) DeserializeTo(channel_, ex_chars.get(), ex_length, 1, exception);
  }
  return value;
}


void ObjectProxy::GetAllPropertyNames(uint32_t* property_count,
                                      PP_Var** properties,
                                      PP_Var* exception) {
  DebugPrintf("ObjectProxy::GetAllPropertyNames\n");
  // TODO(sehr): implement GetAllPropertyNames.
  UNREFERENCED_PARAMETER(property_count);
  UNREFERENCED_PARAMETER(properties);
  UNREFERENCED_PARAMETER(exception);
}


void ObjectProxy::SetProperty(PP_Var name,
                              PP_Var value,
                              PP_Var* exception) {
  DebugPrintf("ObjectProxy::SetProperty\n");
  uint32_t name_length = kMaxVarSize;
  nacl::scoped_array<char> name_chars(Serialize(&name, 1, &name_length));
  if (name_chars == NULL) {
    return;
  }
  uint32_t value_length = kMaxVarSize;
  nacl::scoped_array<char> value_chars(Serialize(&value, 1, &value_length));
  if (value_chars == NULL) {
    return;
  }
  uint32_t ex_in_length = kMaxVarSize;
  nacl::scoped_array<char> ex_in_chars(Serialize(exception, 1, &ex_in_length));
  if (ex_in_chars == NULL) {
    return;
  }
  uint32_t ex_length = kMaxVarSize;
  nacl::scoped_array<char> ex_chars(new char[kMaxVarSize]);
  NaClSrpcError retval =
      ObjectStubRpcClient::SetProperty(
          channel_,
          sizeof(ObjectCapability),
          reinterpret_cast<char*>(&capability_),
          name_length,
          name_chars.get(),
          value_length,
          value_chars.get(),
          ex_in_length,
          ex_in_chars.get(),
          &ex_length,
          ex_chars.get());
  if (retval != NACL_SRPC_RESULT_OK) {
    return;
  }
  if (exception != NULL) {
    (void) DeserializeTo(channel_, ex_chars.get(), ex_length, 1, exception);
  }
}


void ObjectProxy::RemoveProperty(PP_Var name,
                                 PP_Var* exception) {
  DebugPrintf("ObjectProxy::RemoveProperty\n");
  uint32_t name_length = kMaxVarSize;
  nacl::scoped_array<char> name_chars(Serialize(&name, 1, &name_length));
  if (name_chars == NULL) {
    return;
  }
  uint32_t ex_in_length = kMaxVarSize;
  nacl::scoped_array<char> ex_in_chars(Serialize(exception, 1, &ex_in_length));
  if (ex_in_chars == NULL) {
    return;
  }
  uint32_t ex_length = kMaxVarSize;
  nacl::scoped_array<char> ex_chars(new char[kMaxVarSize]);
  NaClSrpcError retval =
      ObjectStubRpcClient::RemoveProperty(
          channel_,
          sizeof(ObjectCapability),
          reinterpret_cast<char*>(&capability_),
          name_length,
          name_chars.get(),
          ex_in_length,
          ex_in_chars.get(),
          &ex_length,
          ex_chars.get());
  if (retval != NACL_SRPC_RESULT_OK) {
    return;
  }
  if (exception != NULL) {
    (void) DeserializeTo(channel_, ex_chars.get(), ex_length, 1, exception);
  }
}


PP_Var ObjectProxy::Call(PP_Var method_name,
                         uint32_t argc,
                         PP_Var* argv,
                         PP_Var* exception) {
  DebugPrintf("ObjectProxy::Call\n");
  PP_Var ret = PP_MakeUndefined();
  uint32_t name_length = kMaxVarSize;
  nacl::scoped_array<char> name_chars(Serialize(&method_name, 1, &name_length));
  if (name_chars == NULL) {
    return ret;
  }
  uint32_t argv_length = kMaxVarSize;
  nacl::scoped_array<char> argv_chars(Serialize(argv, argc, &argv_length));
  // |argv_chars| can be NULL only if |argc| is 0, otherwise an error occurred.
  if (argv_chars == NULL && argc > 0) {
    return ret;
  }
  uint32_t ex_in_length = kMaxVarSize;
  nacl::scoped_array<char> ex_in_chars(Serialize(exception, 1, &ex_in_length));
  if (ex_in_chars == NULL && exception != NULL) {
    return ret;
  }
  uint32_t ex_length = kMaxVarSize;
  nacl::scoped_array<char> ex_chars(new char[kMaxVarSize]);
  uint32_t ret_length = kMaxVarSize;
  nacl::scoped_array<char> ret_chars(new char[kMaxVarSize]);
  NaClSrpcError retval =
      ObjectStubRpcClient::Call(
          channel_,
          sizeof(ObjectCapability),
          reinterpret_cast<char*>(&capability_),
          name_length,
          name_chars.get(),
          static_cast<int32_t>(argc),
          argv_length,
          argv_chars.get(),
          ex_in_length,
          ex_in_chars.get(),
          &ret_length,
          ret_chars.get(),
          &ex_length,
          ex_chars.get());
  if (retval != NACL_SRPC_RESULT_OK) {
    return ret;
  }
  if (!DeserializeTo(channel_, ret_chars.get(), ret_length, 1, &ret)) {
    return ret;
  }
  if (exception != NULL) {
    (void) DeserializeTo(channel_, ex_chars.get(), ex_length, 1, exception);
  }
  return ret;
}


PP_Var ObjectProxy::Construct(uint32_t argc,
                              PP_Var* argv,
                              PP_Var* exception) {
  DebugPrintf("ObjectProxy::Construct\n");
  PP_Var ret = PP_MakeUndefined();
  uint32_t argv_length = kMaxVarSize;
  nacl::scoped_array<char> argv_chars(Serialize(argv, argc, &argv_length));
  if (argv_chars == NULL) {
    return ret;
  }
  uint32_t ex_in_length = kMaxVarSize;
  nacl::scoped_array<char> ex_in_chars(Serialize(exception, 1, &ex_in_length));
  if (ex_in_chars == NULL) {
    return ret;
  }
  uint32_t ex_length = kMaxVarSize;
  nacl::scoped_array<char> ex_chars(new char[kMaxVarSize]);
  uint32_t ret_length = kMaxVarSize;
  nacl::scoped_array<char> ret_chars(new char[kMaxVarSize]);
  NaClSrpcError retval =
      ObjectStubRpcClient::Construct(
          channel_,
          sizeof(ObjectCapability),
          reinterpret_cast<char*>(&capability_),
          static_cast<int32_t>(argc),
          argv_length,
          argv_chars.get(),
          ex_in_length,
          ex_in_chars.get(),
          &ret_length,
          ret_chars.get(),
          &ex_length,
          ex_chars.get());
  if (retval != NACL_SRPC_RESULT_OK) {
    return ret;
  }
  if (!DeserializeTo(channel_, ret_chars.get(), ret_length, 1, &ret)) {
    return ret;
  }
  if (exception != NULL) {
    (void) DeserializeTo(channel_, ex_chars.get(), ex_length, 1, exception);
  }
  return ret;
}


void ObjectProxy::Deallocate() {
  DebugPrintf("ObjectProxy::Deallocate\n");
}


PP_Var ObjectProxy::New(const ObjectCapability& capability,
                        NaClSrpcChannel* channel) {
  if (capability_proxy_map == NULL) {
    capability_proxy_map = new std::map<ObjectCapability, PP_Var*>;
  }
  if ((*capability_proxy_map)[capability] != NULL) {
    PP_Var var = *(*capability_proxy_map)[capability];
    // TODO(sehr): increment the ref count here.
    return var;
  }
  if (capability.pid() == GETPID()) {
    // The object was not in the map, but is local to this process.
    PP_Var var;
    var.type = PP_VARTYPE_OBJECT;
    var.value.as_id = capability.object_id();
    // TODO(sehr): increment the ref count of the object in var here.
    return var;
  }
  Object* proxy = static_cast<Object*>(new ObjectProxy(capability, channel));
  PP_Var* var = new PP_Var;
  *var = VarInterface()->CreateObject(LookupModuleIdForSrpcChannel(channel),
                                      &Object::object_class,
                                      proxy);
  (*capability_proxy_map)[capability] = var;
  // TODO(sehr): increment the ref count of the object in var here.
  return *var;
}

}  // namespace ppapi_proxy
