/*
  Copyright (c) 2011 The Chromium Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/plugin_callback.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/plugin_nacl_file.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_errors.h"
#include "srpcgen/ppb_rpc.h"

namespace ppapi_proxy {

int32_t StreamAsFile(PP_Instance instance,
                     const char* url,
                     struct PP_CompletionCallback callback) {
  DebugPrintf("NaClFile::StreamAsFile: instance=%"NACL_PRIu32" url=%s\n",
              instance, url);

  int32_t callback_id =
      CompletionCallbackTable::Get()->AddCallback(callback);
  if (callback_id == 0)
    return PP_ERROR_BLOCKS_MAIN_THREAD;

  NaClSrpcError srpc_result = NaClFileRpcClient::StreamAsFile(
      GetMainSrpcChannel(), instance, const_cast<char*>(url), callback_id);
  DebugPrintf("NaClFile::StreamAsFile: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return PP_OK_COMPLETIONPENDING;
  return MayForceCallback(callback, PP_ERROR_FAILED);
}


int GetFileDesc(PP_Instance instance, const char* url) {
  DebugPrintf("NaClFile::GetFileDesc: instance=%"NACL_PRIu32" url=%s\n",
              instance, url);

  int file_desc;
  NaClSrpcError srpc_result = NaClFileRpcClient::GetFileDesc(
      GetMainSrpcChannel(), instance, const_cast<char*>(url), &file_desc);
  DebugPrintf("NaClFile::GetFileDesc: %s\n", NaClSrpcErrorString(srpc_result));

  if (srpc_result == NACL_SRPC_RESULT_OK)
    return file_desc;
  return NACL_NO_FILE_DESC;
}

}  // namespace ppapi_proxy
