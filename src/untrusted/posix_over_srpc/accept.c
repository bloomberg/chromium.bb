/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>

#include "posix_over_srpc.h"

struct sockaddr {
    unsigned short    sa_family;    // address family, AF_xxx
    char              sa_data[14];  // 14 bytes of protocol address
};

typedef unsigned long socklen_t;

int accept(int socket, struct sockaddr* address, socklen_t* address_len) {
  __psrpc_request_t request;
  if (0 != __psrpc_request_create(&request, PSRPC_ACCEPT)) {
    return -1;
  }

  request.args[0][0]->u.ival = fd_list_get_key(socket);

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

