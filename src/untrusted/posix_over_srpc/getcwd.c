/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>

#include "native_client/src/untrusted/posix_over_srpc/posix_over_srpc.h"

char* getcwd(char* buf, size_t size) {
  __psrpc_request_t request;
  if (0 != __psrpc_request_create(&request, PSRPC_GETCWD)) {
    return NULL;
  }

  __psrpc_pass_memory(request.args[1][0], buf, size);

  __psrpc_make_call(&request);

  char* result;
  if (0 == request.args[1][1]->u.ival) {
    errno = request.args[1][2]->u.ival;
    result = NULL;
  } else {
    result = buf;
  }

  __psrpc_request_destroy(&request);
  return result;
}
