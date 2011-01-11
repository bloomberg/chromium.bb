// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;
using ppapi_proxy::GetBrowserInterface;

void PpbRpcServer::PPB_GetInterface(NaClSrpcRpc* rpc,
                                    NaClSrpcClosure* done,
                                    char* interface_name,
                                    int32_t* exports_interface_name) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  // Since the proxy will make calls to proxied interfaces, we need simply
  // to know whether the plugin exports a given interface.
  const void* browser_interface = GetBrowserInterface(interface_name);
  DebugPrintf("PPB_GetInterface('%s'): %p\n",
              interface_name, browser_interface);
  *exports_interface_name = (browser_interface != NULL);
  rpc->result = NACL_SRPC_RESULT_OK;
}
