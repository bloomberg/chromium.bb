/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>

#include "native_client/src/untrusted/posix_over_srpc/posix_over_srpc.h"

long pathconf(const char* path, int name) {
  __psrpc_request_t request;
  if (0 != __psrpc_request_create(&request, PSRPC_PATHCONF)) {
    return -1;
  }

  request.args[0][0]->u.sval = (char*)path;
  request.args[0][1]->u.ival = name;

  __psrpc_make_call(&request);

  long result = request.args[1][0]->u.ival;
  if (result == -1) {
    errno = request.args[1][1]->u.ival;
  }

  __psrpc_request_destroy(&request);
  return result;
}
