/*
 * Copyright (c) 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "gen/native_client/src/shared/ppapi_proxy/ppp_rpc.h"

// PPAPI plugins are actually "hosted" by ppruntime.  This is because the
// library needs to start an SRPC loop to dispatch to the stubs.

int main(int argc, char* argv[]) {
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientConnection(PppRpcs::srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();

  return 0;
}
