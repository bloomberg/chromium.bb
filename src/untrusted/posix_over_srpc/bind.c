/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>

#include "native_client/src/untrusted/posix_over_srpc/posix_over_srpc.h"

struct sockaddr {
  unsigned short sa_family;
  char sa_data[14];
};

typedef unsigned long socklen_t;

int bind(int socket, const struct sockaddr* address, socklen_t address_len) {
  __psrpc_request_t request;
  if (0 != __psrpc_request_create(&request, PSRPC_BIND)) {
    return -1;
  }

  request.args[0][0]->u.ival = fd_list_get_key(socket);
  request.args[0][1]->u.ival = address->sa_family;
  __psrpc_pass_memory(request.args[0][2], (void*)address->sa_data, 14);
  request.args[0][3]->u.ival = address_len;

  __psrpc_make_call(&request);

  int result = request.args[1][0]->u.ival;
  if (0 != result) {
    errno = request.args[1][1]->u.ival;
  }

  __psrpc_request_destroy(&request);
  return result;
}
