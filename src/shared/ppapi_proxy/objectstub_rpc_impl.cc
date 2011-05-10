// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_process.h"
#ifdef __native_client__
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#else
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#endif  // __native_client__
#include "native_client/src/shared/ppapi_proxy/object_capability.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_var.h"
#include "srpcgen/ppb_rpc.h"
#include "srpcgen/ppp_rpc.h"

//
// These methods provide dispatching to the implementation of the object stubs.
//

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::ObjectCapability;
using ppapi_proxy::DeserializeTo;
using ppapi_proxy::SerializeTo;
using ppapi_proxy::PPBVarInterface;

namespace {

PP_Var LookupCapability(const ObjectCapability* capability) {
  if (capability->pid() != GETPID()) {
    DebugPrintf("WHOA! PIDS DON'T MATCH\n");
  }
  PP_Var var;
  var.type = PP_VARTYPE_OBJECT;
  var.value.as_id = capability->object_id();
  return var;
}

}  // namespace

void ObjectStubRpcServer::HasProperty(NaClSrpcRpc* rpc,
                                      NaClSrpcClosure* done,
                                      uint32_t capability_length,
                                      char* capability_bytes,
                                      uint32_t name_length,
                                      char* name_bytes,
                                      uint32_t ex_in_length,
                                      char* ex_in_bytes,
                                      int32_t* success,
                                      uint32_t* exception_length,
                                      char* exception_bytes) {
  DebugPrintf("ObjectStubRpcServer::HasProperty\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // Get the receiver object.
  if (capability_length != sizeof(ObjectCapability)) {
    return;
  }
  PP_Var var =
      LookupCapability(reinterpret_cast<ObjectCapability*>(capability_bytes));
  // Get the name PP_Var.
  PP_Var name;
  if (!DeserializeTo(rpc->channel, name_bytes, name_length, 1, &name)) {
    // Deserialization of name failed.
    return;
  }
  // Get the previous value of the exception PP_Var.
  PP_Var exception;
  if (ex_in_length != 0) {
    if (!DeserializeTo(rpc->channel,
                       ex_in_bytes,
                       ex_in_length,
                       1,
                       &exception)) {
      // Deserialization of exception failed.
      return;
    }
  }
  // Invoke the method.
  *success = PPBVarInterface()->HasProperty(var, name, &exception);
  // Return the final value of the exception PP_Var.
  if (!SerializeTo(&exception, exception_bytes, exception_length)) {
    // Serialization of exception failed.
    return;
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}


void ObjectStubRpcServer::HasMethod(NaClSrpcRpc* rpc,
                                    NaClSrpcClosure* done,
                                    uint32_t capability_length,
                                    char* capability_bytes,
                                    uint32_t name_length,
                                    char* name_bytes,
                                    uint32_t ex_in_length,
                                    char* ex_in_bytes,
                                    int32_t* success,
                                    uint32_t* exception_length,
                                    char* exception_bytes) {
  DebugPrintf("ObjectStubRpcServer::HasMethod\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // Get the receiver object.
  if (capability_length != sizeof(ObjectCapability)) {
    return;
  }
  PP_Var var =
      LookupCapability(reinterpret_cast<ObjectCapability*>(capability_bytes));
  // Get the name PP_Var.
  PP_Var name;
  if (!DeserializeTo(rpc->channel, name_bytes, name_length, 1, &name)) {
    // Deserialization of name failed.
    return;
  }
  // Get the previous value of the exception PP_Var.
  PP_Var exception;
  if (ex_in_length != 0) {
    if (!DeserializeTo(rpc->channel,
                       ex_in_bytes,
                       ex_in_length,
                       1,
                       &exception)) {
      // Deserialization of exception failed.
      return;
    }
  }
  // Invoke the method.
  *success = PPBVarInterface()->HasMethod(var, name, &exception);
  // Return the final value of the exception PP_Var.
  if (!SerializeTo(&exception, exception_bytes, exception_length)) {
    // Serialization of exception failed.
    return;
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}


void ObjectStubRpcServer::GetProperty(NaClSrpcRpc* rpc,
                                      NaClSrpcClosure* done,
                                      uint32_t capability_length,
                                      char* capability_bytes,
                                      uint32_t name_length,
                                      char* name_bytes,
                                      uint32_t ex_in_length,
                                      char* ex_in_bytes,
                                      uint32_t* value_length,
                                      char* value_bytes,
                                      uint32_t* exception_length,
                                      char* exception_bytes) {
  DebugPrintf("ObjectStubRpcServer::GetProperty\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // Get the receiver object.
  if (capability_length != sizeof(ObjectCapability)) {
    return;
  }
  PP_Var var =
      LookupCapability(reinterpret_cast<ObjectCapability*>(capability_bytes));
  // Get the name PP_Var.
  PP_Var name;
  if (!DeserializeTo(rpc->channel, name_bytes, name_length, 1, &name)) {
    // Deserialization of name failed.
    return;
  }
  // Get the previous value of the exception PP_Var.
  PP_Var exception;
  if (ex_in_length != 0) {
    if (!DeserializeTo(rpc->channel,
                       ex_in_bytes,
                       ex_in_length,
                       1,
                       &exception)) {
      // Deserialization of exception failed.
      return;
    }
  }
  // Invoke the method.
  PP_Var value = PPBVarInterface()->GetProperty(var, name, &exception);
  // Return the value PP_Var.
  if (!SerializeTo(&value, value_bytes, value_length)) {
    // Serialization of value failed.
    return;
  }
  // Return the final value of the exception PP_Var.
  if (!SerializeTo(&exception, exception_bytes, exception_length)) {
    // Serialization of exception failed.
    return;
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}


void ObjectStubRpcServer::GetAllPropertyNames(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    uint32_t capability_length,
    char* capability_bytes,
    uint32_t ex_in_length,
    char* ex_in_bytes,
    int32_t* property_count,
    uint32_t* properties_length,
    char* properties_bytes,
    uint32_t* exception_length,
    char* exception_bytes) {
  DebugPrintf("ObjectStubRpcServer::GetAllPropertyNames\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // Get the receiver object.
  if (capability_length != sizeof(ObjectCapability)) {
    return;
  }
  PP_Var var =
      LookupCapability(reinterpret_cast<ObjectCapability*>(capability_bytes));
  // Get the previous value of the exception PP_Var.
  PP_Var exception;
  if (!DeserializeTo(rpc->channel, ex_in_bytes, ex_in_length, 1, &exception)) {
    // Deserialization of exception failed.
    return;
  }
  UNREFERENCED_PARAMETER(property_count);
  UNREFERENCED_PARAMETER(properties_length);
  UNREFERENCED_PARAMETER(properties_bytes);
  // Invoke the method.
  // TODO(sehr): implement GetAllPropertyNames.
  // Return the final value of the exception PP_Var.
  if (!SerializeTo(&exception, exception_bytes, exception_length)) {
    // Serialization of exception failed.
    return;
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}


void ObjectStubRpcServer::SetProperty(NaClSrpcRpc* rpc,
                                      NaClSrpcClosure* done,
                                      uint32_t capability_length,
                                      char* capability_bytes,
                                      uint32_t name_length,
                                      char* name_bytes,
                                      uint32_t value_length,
                                      char* value_bytes,
                                      uint32_t ex_in_length,
                                      char* ex_in_bytes,
                                      uint32_t* exception_length,
                                      char* exception_bytes) {
  DebugPrintf("ObjectStubRpcServer::SetProperty\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // Get the receiver object.
  if (capability_length != sizeof(ObjectCapability)) {
    return;
  }
  PP_Var var =
      LookupCapability(reinterpret_cast<ObjectCapability*>(capability_bytes));
  // Get the name PP_Var.
  PP_Var name;
  if (!DeserializeTo(rpc->channel, name_bytes, name_length, 1, &name)) {
    // Deserialization of name failed.
    return;
  }
  // Get the value PP_Var.
  PP_Var value;
  if (!DeserializeTo(rpc->channel, value_bytes, value_length, 1, &value)) {
    // Deserialization of value failed.
    return;
  }
  // Get the previous exception PP_Var.
  PP_Var exception;
  if (ex_in_length != 0) {
    if (!DeserializeTo(rpc->channel,
                       ex_in_bytes,
                       ex_in_length,
                       1,
                       &exception)) {
      // Deserialization of exception failed.
      return;
    }
  }
  // Invoke the method.
  PPBVarInterface()->SetProperty(var, name, value, &exception);
  // Return the final value of the exception PP_Var.
  if (!SerializeTo(&exception, exception_bytes, exception_length)) {
    // Serialization of exception failed.
    return;
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}


void ObjectStubRpcServer::RemoveProperty(NaClSrpcRpc* rpc,
                                         NaClSrpcClosure* done,
                                         uint32_t capability_length,
                                         char* capability_bytes,
                                         uint32_t name_length,
                                         char* name_bytes,
                                         uint32_t ex_in_length,
                                         char* ex_in_bytes,
                                         uint32_t* exception_length,
                                         char* exception_bytes) {
  DebugPrintf("ObjectStubRpcServer::RemoveProperty\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // Get the receiver object.
  if (capability_length != sizeof(ObjectCapability)) {
    return;
  }
  PP_Var var =
      LookupCapability(reinterpret_cast<ObjectCapability*>(capability_bytes));
  // Get the name PP_Var.
  PP_Var name;
  if (!DeserializeTo(rpc->channel, name_bytes, name_length, 1, &name)) {
    // Deserialization of name failed.
    return;
  }
  // Get the previous value of the exception PP_Var.
  PP_Var exception;
  if (ex_in_length != 0) {
    if (!DeserializeTo(rpc->channel,
                       ex_in_bytes,
                       ex_in_length,
                       1,
                       &exception)) {
      // Deserialization of exception failed.
      return;
    }
  }
  // Invoke the method.
  PPBVarInterface()->RemoveProperty(var, name, &exception);
  // Return the final value of the exception PP_Var.
  if (!SerializeTo(&exception, exception_bytes, exception_length)) {
    // Serialization of exception failed.
    return;
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}


void ObjectStubRpcServer::Call(NaClSrpcRpc* rpc,
                               NaClSrpcClosure* done,
                               uint32_t capability_length,
                               char* capability_bytes,
                               uint32_t name_length,
                               char* name_bytes,
                               int32_t argc,
                               uint32_t argv_length,
                               char* argv_bytes,
                               uint32_t ex_in_length,
                               char* ex_in_bytes,
                               uint32_t* ret_length,
                               char* ret_bytes,
                               uint32_t* exception_length,
                               char* exception_bytes) {
  DebugPrintf("ObjectStubRpcServer::Call\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // Get the receiver object.
  if (capability_length != sizeof(ObjectCapability)) {
    return;
  }
  PP_Var var =
      LookupCapability(reinterpret_cast<ObjectCapability*>(capability_bytes));
  // Get the previous value of the exception PP_Var.
  PP_Var exception;
  if (ex_in_length != 0) {
    if (!DeserializeTo(rpc->channel,
                       ex_in_bytes,
                       ex_in_length,
                       1,
                       &exception)) {
      // Deserialization of exception failed.
      return;
    }
  }
  // Get the name PP_Var.
  PP_Var name;
  if (!DeserializeTo(rpc->channel, name_bytes, name_length, 1, &name)) {
    // Deserialization of name failed.
    return;
  }
  // Get the parameters.
  nacl::scoped_array<PP_Var> argv(new PP_Var[argc]);
  if (!DeserializeTo(rpc->channel,
                     argv_bytes,
                     argv_length,
                     argc,
                     argv.get())) {
    // Deserialization of argv failed.
    return;
  }
  // Invoke the method.
  PP_Var ret = PPBVarInterface()->Call(var,
                                       name,
                                       static_cast<uint32_t>(argc),
                                       argv.get(),
                                       &exception);
  // Return ret.
  if (!SerializeTo(&ret, ret_bytes, ret_length)) {
    // Serialization of ret failed.
    return;
  }
  // Return the final value of the exception PP_Var.
  if (!SerializeTo(&exception, exception_bytes, exception_length)) {
    // Serialization of exception failed.
    return;
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}

void ObjectStubRpcServer::Construct(NaClSrpcRpc* rpc,
                                    NaClSrpcClosure* done,
                                    uint32_t capability_length,
                                    char* capability_bytes,
                                    int32_t argc,
                                    uint32_t argv_length,
                                    char* argv_bytes,
                                    uint32_t ex_in_length,
                                    char* ex_in_bytes,
                                    uint32_t* ret_length,
                                    char* ret_bytes,
                                    uint32_t* exception_length,
                                    char* exception_bytes) {
  DebugPrintf("ObjectStubRpcServer::Construct\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // Get the receiver object.
  if (capability_length != sizeof(ObjectCapability)) {
    return;
  }
  PP_Var var =
      LookupCapability(reinterpret_cast<ObjectCapability*>(capability_bytes));
  // Get the previous value of the exception PP_Var.
  PP_Var exception;
  if (ex_in_length != 0) {
    if (!DeserializeTo(rpc->channel,
                       ex_in_bytes,
                       ex_in_length,
                       1,
                       &exception)) {
      // Deserialization of exception failed.
      return;
    }
  }
  nacl::scoped_array<PP_Var> argv(new PP_Var[argc]);
  if (!DeserializeTo(rpc->channel,
                     argv_bytes,
                     argv_length,
                     argc,
                     argv.get())) {
    // Deserialization of argv failed.
    return;
  }
  // Invoke the method.
  PP_Var ret = PPBVarInterface()->Construct(var,
                                            static_cast<uint32_t>(argc),
                                            argv.get(),
                                            &exception);
  // Return ret.
  if (!SerializeTo(&ret, ret_bytes, ret_length)) {
    // Serialization of ret failed.
    return;
  }
  // Return the final value of the exception PP_Var.
  if (!SerializeTo(&exception, exception_bytes, exception_length)) {
    // Serialization of exception failed.
    return;
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}


void ObjectStubRpcServer::Deallocate(NaClSrpcRpc* rpc,
                                     NaClSrpcClosure* done,
                                     uint32_t capability_length,
                                     char* capability_bytes) {
  DebugPrintf("ObjectStubRpcServer::Deallocate\n");
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // Get the receiver object.
  if (capability_length != sizeof(ObjectCapability)) {
    return;
  }
  PP_Var var =
      LookupCapability(reinterpret_cast<ObjectCapability*>(capability_bytes));
  // Invoke the method.
  rpc->result = NACL_SRPC_RESULT_OK;
}
