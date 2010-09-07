/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <unistd.h>

#include "posix_over_srpc.h"

int nacl_close(int fd) {
  int result = close(fd);
  __psrpc_request_t request;
  if (0 != __psrpc_request_create(&request, PSRPC_CLOSE)) {
    return result;
  }

  request.args[0][0]->u.ival = fd_list_get_key(fd);
  if (request.args[0][0]->u.ival == -1) {
    return result;
  }
  fd_list_remove(fd);

  __psrpc_make_call(&request);

  __psrpc_request_destroy(&request);
  return result;
}
