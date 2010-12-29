// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "srpcgen/ppb_rpc.h"

//
// The following methods are the SRPC dispatchers for ppapi/c/ppb_core.h.
//

void PpbCoreRpcServer::PPB_Core_AddRefResource(NaClSrpcRpc* rpc,
                                               NaClSrpcClosure* done,
                                               PP_Resource resource) {
  NaClSrpcClosureRunner runner(done);
  ppapi_proxy::PPBCoreInterface()->AddRefResource(resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

void PpbCoreRpcServer::PPB_Core_ReleaseResource(NaClSrpcRpc* rpc,
                                                NaClSrpcClosure* done,
                                                PP_Resource resource) {
  NaClSrpcClosureRunner runner(done);
  ppapi_proxy::PPBCoreInterface()->ReleaseResource(resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}

// MemAlloc and MemFree are handled locally to the plugin, and do not need a
// browser stub.

void PpbCoreRpcServer::PPB_Core_GetTime(NaClSrpcRpc* rpc,
                                        NaClSrpcClosure* done,
                                        double* time) {
  NaClSrpcClosureRunner runner(done);
  *time = ppapi_proxy::PPBCoreInterface()->GetTime();
  rpc->result = NACL_SRPC_RESULT_OK;
}

// Release multiple references at once.
void PpbCoreRpcServer::ReleaseResourceMultipleTimes(NaClSrpcRpc* rpc,
                                                    NaClSrpcClosure* done,
                                                    PP_Resource resource,
                                                    int32_t count) {
  NaClSrpcClosureRunner runner(done);
  while (count--)
    ppapi_proxy::PPBCoreInterface()->ReleaseResource(resource);
  rpc->result = NACL_SRPC_RESULT_OK;
}


// CallOnMainThread is handled on the upcall thread, where another RPC service
// is exported.
//
// IsMainThread is handled locally to the plugin, and does not need a browser
// stub.
