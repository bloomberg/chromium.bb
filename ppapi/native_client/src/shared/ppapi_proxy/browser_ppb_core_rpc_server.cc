// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_Core functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/pp_completion_callback.h"
#include "srpcgen/ppb_rpc.h"
#include "srpcgen/upcall.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPBCoreInterface;
using ppapi_proxy::MakeRemoteCompletionCallback;
using ppapi_proxy::GetMainSrpcChannel;

void PpbCoreRpcServer::PPB_Core_AddRefResource(NaClSrpcRpc* rpc,
                                               NaClSrpcClosure* done,
                                               PP_Resource resource) {
  NaClSrpcClosureRunner runner(done);
  PPBCoreInterface()->AddRefResource(resource);
  DebugPrintf("PPB_Core::AddRefResource: resource=%"NACL_PRIu32"\n", resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbCoreRpcServer::PPB_Core_ReleaseResource(NaClSrpcRpc* rpc,
                                                NaClSrpcClosure* done,
                                                PP_Resource resource) {
  NaClSrpcClosureRunner runner(done);
  PPBCoreInterface()->ReleaseResource(resource);
  DebugPrintf("PPB_Core::ReleaseResource: resource=%"NACL_PRIu32"\n",
              resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

// MemAlloc and MemFree are handled locally to the plugin, and do not need a
// browser stub.

void PpbCoreRpcServer::PPB_Core_GetTime(NaClSrpcRpc* rpc,
                                        NaClSrpcClosure* done,
                                        double* time) {
  NaClSrpcClosureRunner runner(done);
  *time = PPBCoreInterface()->GetTime();
  DebugPrintf("PPB_Core::GetTime: time=%f\n", *time);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbCoreRpcServer::PPB_Core_GetTimeTicks(NaClSrpcRpc* rpc,
                                             NaClSrpcClosure* done,
                                             double* time_ticks) {
  NaClSrpcClosureRunner runner(done);
  *time_ticks = PPBCoreInterface()->GetTimeTicks();
  DebugPrintf("PPB_Core::GetTimeTicks: time_ticks=%f\n", *time_ticks);
  rpc->result = NACL_SRPC_RESULT_OK;
}

// Release multiple references at once.
void PpbCoreRpcServer::ReleaseResourceMultipleTimes(NaClSrpcRpc* rpc,
                                                    NaClSrpcClosure* done,
                                                    PP_Resource resource,
                                                    int32_t count) {
  NaClSrpcClosureRunner runner(done);
  while (count--)
    PPBCoreInterface()->ReleaseResource(resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

// Invoked from main thread.
void PpbCoreRpcServer::PPB_Core_CallOnMainThread(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t delay_in_milliseconds,
    int32_t callback_id,
    int32_t result) {
  CHECK(PPBCoreInterface()->IsMainThread());
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback =
      MakeRemoteCompletionCallback(rpc->channel, callback_id);
  if (remote_callback.func == NULL)
    return;  // Treat this as a generic SRPC error.

  PPBCoreInterface()->CallOnMainThread(
      delay_in_milliseconds, remote_callback, result);
  DebugPrintf("PPB_Core::CallOnMainThread_main: "
              "delay_in_milliseconds=%"NACL_PRId32"\n", delay_in_milliseconds);
  rpc->result = NACL_SRPC_RESULT_OK;
}

  // Invoked from upcall thread.
void PppUpcallRpcServer::PPB_Core_CallOnMainThread(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    int32_t delay_in_milliseconds,
    int32_t callback_id,
    int32_t result) {
  CHECK(!PPBCoreInterface()->IsMainThread());
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback_on_main =
      MakeRemoteCompletionCallback(GetMainSrpcChannel(rpc), callback_id);
  if (remote_callback_on_main.func == NULL)
    return;  // Treat this as a generic SRPC error.

  PPBCoreInterface()->CallOnMainThread(
      delay_in_milliseconds, remote_callback_on_main, result);
  DebugPrintf("PPB_Core::CallOnMainThread_upcall: "
              "delay_in_milliseconds=%"NACL_PRId32"\n", delay_in_milliseconds);
  rpc->result = NACL_SRPC_RESULT_OK;
}

// IsMainThread is handled locally to the plugin, and does not need a browser
// stub.
