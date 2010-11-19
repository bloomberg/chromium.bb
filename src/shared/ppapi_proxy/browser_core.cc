// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/portability.h"
#include "srpcgen/ppb_rpc.h"

//
// The following methods are the SRPC dispatchers for ppapi/c/ppb_core.h.
//

void PpbCoreRpcServer::PPB_Core_AddRefResource(NaClSrpcRpc* rpc,
                                               NaClSrpcClosure* done,
                                               int64_t resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // TODO(sehr): implement AddRefResource.
  UNREFERENCED_PARAMETER(resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbCoreRpcServer::PPB_Core_ReleaseResource(NaClSrpcRpc* rpc,
                                                NaClSrpcClosure* done,
                                                int64_t resource) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // TODO(sehr): implement ReleaseResource.
  UNREFERENCED_PARAMETER(resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

// MemAlloc and MemFree are handled locally to the plugin, and do not need a
// browser stub.

void PpbCoreRpcServer::PPB_Core_GetTime(NaClSrpcRpc* rpc,
                                        NaClSrpcClosure* done,
                                        double* time) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // TODO(sehr): implement time.
  *time = 0.0;
  rpc->result = NACL_SRPC_RESULT_OK;
}

// CallOnMainThread is handled on the upcall thread, where another RPC service
// is exported.
//
// IsMainThread is handled locally to the plugin, and does not need a browser
// stub.
