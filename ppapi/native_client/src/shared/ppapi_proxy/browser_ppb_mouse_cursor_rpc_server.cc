// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SRPC-abstraction wrappers around PPB_MouseCursor functions.

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/ppb_image_data.h"
#include "ppapi/c/ppb_mouse_cursor.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::PPBMouseCursorInterface;

void PpbMouseCursorRpcServer::PPB_MouseCursor_SetCursor(
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
  PP_Bool pp_success = PPBMouseCursorInterface()->SetCursor(
      instance,
      static_cast<PP_MouseCursor_Type>(type),
      custom_image,
      pp_hot_spot);
  *success = PP_ToBool(pp_success);

  DebugPrintf("PPB_MouseCursor::SetCursor: success=%"NACL_PRId32"\n",
              *success);
  rpc->result = NACL_SRPC_RESULT_OK;
}
