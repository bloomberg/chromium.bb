/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>

#include "native_client/src/untrusted/posix_over_srpc/posix_over_srpc.h"

typedef unsigned long socklen_t;

int setsockopt(int socket, int level, int option_name, const void* option_value,
               socklen_t option_len) {
  __psrpc_request_t request;
  if (0 != __psrpc_request_create(&request, PSRPC_SETSOCKOPT)) {
    return -1;
  }

  request.args[0][0]->u.ival = fd_list_get_key(socket);
  request.args[0][1]->u.ival = level;
  request.args[0][2]->u.ival = option_name;
  __psrpc_pass_memory(request.args[0][3], (void*)option_value, option_len);

  __psrpc_make_call(&request);

  int result = request.args[1][0]->u.ival;
  if (0 != result) {
    errno = request.args[1][1]->u.ival;
  }

  __psrpc_request_destroy(&request);
  return result;
}
