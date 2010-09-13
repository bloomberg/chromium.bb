// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/portability.h"
#include "gen/native_client/src/shared/ppapi_proxy/ppb_rpc.h"

//
// The following methods are the SRPC dispatchers for ppapi/c/ppb_core.h.
//

NaClSrpcError PpbCoreRpcServer::PPB_Core_AddRefResource(
    NaClSrpcChannel* channel,
    int64_t resource) {
  // TODO(sehr): implement AddRefResource.
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(resource);
  return NACL_SRPC_RESULT_OK;
}

NaClSrpcError PpbCoreRpcServer::PPB_Core_ReleaseResource(
    NaClSrpcChannel* channel,
    int64_t resource) {
  // TODO(sehr): implement ReleaseResource.
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(resource);
  return NACL_SRPC_RESULT_OK;
}

// MemAlloc and MemFree are handled locally to the plugin, and do not need a
// browser stub.

NaClSrpcError PpbCoreRpcServer::PPB_Core_GetTime(
    NaClSrpcChannel* channel,
    double* time) {
  UNREFERENCED_PARAMETER(channel);
  // TODO(sehr): implement time.
  *time = 0.0;
  return NACL_SRPC_RESULT_OK;
}

// CallOnMainThread is handled on the upcall thread, where another RPC service
// is exported.
//
// IsMainThread is handled locally to the plugin, and does not need a browser
// stub.
