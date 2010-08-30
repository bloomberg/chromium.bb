/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>

#include "posix_over_srpc.h"

int nacl_open(const char *path, int flags, ...) {
  __psrpc_request_t request;
  if (0 != __psrpc_request_create(&request, PSRPC_OPEN)) {
    return -1;
  }

  va_list ap;
  mode_t mode = 0;
  va_start(ap, flags);
  mode = va_arg(ap, mode_t);
  request.args[0][0]->u.sval = (char*)path;
  request.args[0][1]->u.ival = flags;
  request.args[0][2]->u.ival = mode;

  __psrpc_make_call(&request);

  int result = request.args[1][0]->u.hval;
  if (-1 == result) {
    errno = request.args[1][1]->u.ival;
  }

  __psrpc_request_destroy(&request);
  return result;
}
