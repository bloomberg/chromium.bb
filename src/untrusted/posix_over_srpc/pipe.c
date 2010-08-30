/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>

#include "posix_over_srpc.h"

int pipe(int fd[2]) {
  __psrpc_request_t request;
  if (0 != __psrpc_request_create(&request, PSRPC_PIPE)) {
    return -1;
  }

  __psrpc_make_call(&request);

  int result = request.args[1][0]->u.ival;
  if (-1 == result) {
    errno = request.args[1][1]->u.ival;
  } else {
    fd[0] = request.args[1][2]->u.hval;
    fd[1] = request.args[1][3]->u.hval;
  }

  __psrpc_request_destroy(&request);
  return result;
}
