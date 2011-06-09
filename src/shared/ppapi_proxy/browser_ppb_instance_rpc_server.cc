// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Instance functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/object_serialize.h"
#include "srpcgen/ppb_rpc.h"

//TODO(dmichael): Remove this function from the SRPC interface and here when
//                scripting functions are removed from PPB_Instance.
void PpbInstanceRpcServer::PPB_Instance_GetWindowObject(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    // outputs
    nacl_abi_size_t* window_size, char* window_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
#ifdef PPAPI_INSTANCE_REMOVE_SCRIPTING
  UNREFERENCED_PARAMETER(rpc);
  UNREFERENCED_PARAMETER(done);
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(window_size);
  UNREFERENCED_PARAMETER(window_bytes);
#else
  PP_Var window =
      ppapi_proxy::PPBInstanceInterface()->GetWindowObject(instance);
  if (!ppapi_proxy::SerializeTo(&window, window_bytes, window_size))
    return;

  rpc->result = NACL_SRPC_RESULT_OK;
#endif
}

//TODO(dmichael): Remove this function from the SRPC interface and here.
void PpbInstanceRpcServer::PPB_Instance_GetOwnerElementObject(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    // outputs
    nacl_abi_size_t* owner_size, char* owner_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

#ifdef PPAPI_INSTANCE_REMOVE_SCRIPTING
  UNREFERENCED_PARAMETER(rpc);
  UNREFERENCED_PARAMETER(done);
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(owner_size);
  UNREFERENCED_PARAMETER(owner_bytes);
#else
  PP_Var owner =
      ppapi_proxy::PPBInstanceInterface()->GetOwnerElementObject(instance);
  if (!ppapi_proxy::SerializeTo(&owner, owner_bytes, owner_size))
    return;

  rpc->result = NACL_SRPC_RESULT_OK;
#endif
}

void PpbInstanceRpcServer::PPB_Instance_BindGraphics(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    PP_Resource graphics_device,
    // outputs
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success =
      ppapi_proxy::PPBInstanceInterface()->BindGraphics(
        instance,
        graphics_device);
  *success = (pp_success == PP_TRUE);

  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbInstanceRpcServer::PPB_Instance_IsFullFrame(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    // outputs
    int32_t* is_full_frame) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_is_full_frame =
      ppapi_proxy::PPBInstanceInterface()->IsFullFrame(instance);
  *is_full_frame = (pp_is_full_frame == PP_TRUE);

  rpc->result = NACL_SRPC_RESULT_OK;
}

//TODO(dmichael): Remove this function from the SRPC interface and here.
void PpbInstanceRpcServer::PPB_Instance_ExecuteScript(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    // inputs
    PP_Instance instance,
    nacl_abi_size_t script_size, char* script_bytes,
    nacl_abi_size_t exception_in_size, char* exception_in_bytes,
    // outputs
    nacl_abi_size_t* result_size, char* result_bytes,
    nacl_abi_size_t* exception_size, char* exception_bytes) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

#ifdef PPAPI_INSTANCE_REMOVE_SCRIPTING
  UNREFERENCED_PARAMETER(rpc);
  UNREFERENCED_PARAMETER(done);
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(script_size);
  UNREFERENCED_PARAMETER(script_bytes);
  UNREFERENCED_PARAMETER(exception_in_size);
  UNREFERENCED_PARAMETER(exception_in_bytes);
  UNREFERENCED_PARAMETER(result_size);
  UNREFERENCED_PARAMETER(result_bytes);
  UNREFERENCED_PARAMETER(exception_size);
  UNREFERENCED_PARAMETER(exception_bytes);
#else
  PP_Var script;
  if (!ppapi_proxy::DeserializeTo(
          rpc->channel, script_bytes, script_size, 1, &script))
    return;
  PP_Var exception;
  if (!ppapi_proxy::DeserializeTo(
          rpc->channel, exception_in_bytes, exception_in_size, 1, &exception))
    return;
  PP_Var result =
      ppapi_proxy::PPBInstanceInterface()->ExecuteScript(
          instance, script, &exception);
  if (!ppapi_proxy::SerializeTo(&result, result_bytes, result_size))
    return;
  if (!ppapi_proxy::SerializeTo(&exception, exception_bytes, exception_size))
    return;

  rpc->result = NACL_SRPC_RESULT_OK;
#endif
}
