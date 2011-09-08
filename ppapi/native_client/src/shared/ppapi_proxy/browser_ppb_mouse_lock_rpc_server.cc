// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_MouseLock_Dev functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_callback.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/dev/ppb_mouse_lock_dev.h"
#include "ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::DeleteRemoteCallbackInfo;
using ppapi_proxy::MakeRemoteCompletionCallback;
using ppapi_proxy::PPBMouseLockInterface;

void PpbMouseLockRpcServer::PPB_MouseLock_LockMouse(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t callback_id,
    int32_t* pp_error) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_CompletionCallback remote_callback = MakeRemoteCompletionCallback(
      rpc->channel, callback_id);
  if (NULL == remote_callback.func)
    return;

  *pp_error = PPBMouseLockInterface()->LockMouse(instance, remote_callback);
  DebugPrintf("PPB_MouseLock::LockMouse: pp_error=%"NACL_PRId32"\n", *pp_error);
  CHECK(*pp_error != PP_OK);  // LockMouse should not complete synchronously.

  if (*pp_error != PP_OK_COMPLETIONPENDING)
    DeleteRemoteCallbackInfo(remote_callback);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbMouseLockRpcServer::PPB_MouseLock_UnlockMouse(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_OK;
  DebugPrintf("PPB_MouseLock::UnlockMouse\n");
  PPBMouseLockInterface()->UnlockMouse(instance);
}
