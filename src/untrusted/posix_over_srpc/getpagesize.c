/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/untrusted/posix_over_srpc/posix_over_srpc.h"

int getpagesize() {
  __psrpc_request_t request;
  if (0 != __psrpc_request_create(&request, PSRPC_GETPAGESIZE)) {
    return 0;
  }

  __psrpc_make_call(&request);

  int result_size = request.args[1][0]->u.ival;

  __psrpc_request_destroy(&request);
  return result_size;
}
