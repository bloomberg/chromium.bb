/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>

#include "posix_over_srpc.h"

int listen(int socket, int backlog) {
  __psrpc_request_t request;
  if (0 != __psrpc_request_create(&request, PSRPC_LISTEN)) {
    return -1;
  }

  request.args[0][0]->u.ival = fd_list_get_key(socket);
  request.args[0][1]->u.ival = backlog;

  __psrpc_make_call(&request);

  int result = request.args[1][0]->u.ival;
  if (0 != result) {
    errno = request.args[1][1]->u.ival;
  }

  __psrpc_request_destroy(&request);
  return result;
}
