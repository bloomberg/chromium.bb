/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/fcntl.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/nacl_assert.h"
#include "native_client/src/public/imc_syscalls.h"
#include "native_client/src/public/name_service.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

int main(void) {
  int ns;
  NaClSrpcChannel channel;
  int connected_socket;
  int status;
  int rng;

  if (!NaClSrpcModuleInit()) {
    fprintf(stderr, "srpc module init failed\n");
    return 1;
  }
  printf("Hello world\n");
  ns = -1;
  nacl_nameservice(&ns);
  printf("ns = %d\n", ns);
  assert(-1 != ns);

  connected_socket = imc_connect(ns);
  assert(-1 != connected_socket);
  if (!NaClSrpcClientCtor(&channel, connected_socket)) {
    fprintf(stderr, "Srpc client channel ctor failed\n");
    return 1;
  }
  printf("NaClSrpcClientCtor succeeded\n");
  if (NACL_SRPC_RESULT_OK !=
      NaClSrpcInvokeBySignature(&channel, NACL_NAME_SERVICE_LOOKUP,
                                "SecureRandom", O_RDONLY, &status, &rng)) {
    fprintf(stderr, "nameservice lookup failed, status %d\n", status);
    return 1;
  }
  printf("rpc status %d\n", status);
  /*
   * Now that the "SecureRandom" service has been removed, there is no
   * service that is registered with the name service by default that we
   * can test here.  "ManifestNameService" only gets registered after the
   * "reverse service" is initialized, which doesn't normally happen in
   * standalone sel_ldr.  So we just check that querying returns a sensible
   * error here.
   */
  ASSERT_EQ(NACL_NAME_SERVICE_NAME_NOT_FOUND, status);

  return 0;
}
