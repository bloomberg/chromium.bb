/*
 * Copyright (c) 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "gen/native_client/src/shared/npruntime/npnavigator_rpc.h"

// NPAPI modules are actually "hosted" by the npruntime library, because
// they need an SRPC message processing loop to invoke the stubs.

int main(int argc, char* argv[]) {
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientConnection(NPNavigatorRpcs::srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();

  return 0;
}
