// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_CursorControl_Dev functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/dev/ppb_cursor_control_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/ppb_image_data.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPBCursorControlInterface;

void PpbCursorControlRpcServer::PPB_CursorControl_SetCursor(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t type,
    PP_Resource custom_image,
    nacl_abi_size_t hot_spot_size, char* hot_spot,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Point* pp_hot_spot = NULL;
  if (hot_spot_size != 0) {
    if (hot_spot_size != sizeof(struct PP_Point))
      return;
    pp_hot_spot = reinterpret_cast<struct PP_Point*>(hot_spot);
  }
  PP_Bool pp_success = PPBCursorControlInterface()->SetCursor(
      instance,
      static_cast<PP_CursorType_Dev>(type),
      custom_image,
      pp_hot_spot);
  *success = (pp_success == PP_TRUE);

  DebugPrintf("PPB_CursorControl::SetCursor: success=%"NACL_PRId32"\n",
              *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbCursorControlRpcServer::PPB_CursorControl_LockCursor(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success = PPBCursorControlInterface()->LockCursor(instance);
  *success = (pp_success == PP_TRUE);

  DebugPrintf("PPB_CursorControl::LockCursor: success=%"NACL_PRId32"\n",
              *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbCursorControlRpcServer::PPB_CursorControl_UnlockCursor(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success = PPBCursorControlInterface()->UnlockCursor(instance);
  *success = (pp_success == PP_TRUE);

  DebugPrintf("PPB_CursorControl::UnlockCursor: success=%"NACL_PRId32"\n",
              *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbCursorControlRpcServer::PPB_CursorControl_HasCursorLock(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success = PPBCursorControlInterface()->HasCursorLock(instance);
  *success = (pp_success == PP_TRUE);

  DebugPrintf("PPB_CursorControl::HasCursorLock: success=%"NACL_PRId32"\n",
              *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbCursorControlRpcServer::PPB_CursorControl_CanLockCursor(
    NaClSrpcRpc* rpc,
    NaClSrpcClosure* done,
    PP_Instance instance,
    int32_t* success) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;

  PP_Bool pp_success = PPBCursorControlInterface()->CanLockCursor(instance);
  *success = (pp_success == PP_TRUE);

  DebugPrintf("PPB_CursorControl::CanLockCursor: success=%"NACL_PRId32"\n",
              *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}

