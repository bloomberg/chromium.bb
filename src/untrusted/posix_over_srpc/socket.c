/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>

#include "posix_over_srpc.h"

int socket(int domain, int type, int protocol) {
  __psrpc_request_t request;
  if (0 != __psrpc_request_create(&request, PSRPC_SOCKET)) {
    return -1;
  }

  request.args[0][0]->u.ival = domain;
  request.args[0][1]->u.ival = type;
  request.args[0][2]->u.ival = protocol;

  __psrpc_make_call(&request);

  int result = request.args[1][0]->u.hval;
  if (-1 == result) {
    errno = request.args[1][2]->u.ival;
  } else {
    fd_list_push(result, request.args[1][1]->u.ival);
  }

  __psrpc_request_destroy(&request);
  return result;
}
