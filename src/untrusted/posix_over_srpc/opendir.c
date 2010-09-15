/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>

#include "native_client/src/untrusted/posix_over_srpc/posix_over_srpc.h"

DIR* nacl_opendir(const char* dir_name) {
  __psrpc_request_t request;
  if (0 != __psrpc_request_create(&request, PSRPC_OPENDIR)) {
    return NULL;
  }

  request.args[0][0]->u.sval = (char*)dir_name;

  __psrpc_make_call(&request);

  DIR* result = (DIR*)request.args[1][0]->u.ival;
  if (NULL == result) {
    errno = request.args[1][1]->u.ival;
  }

  __psrpc_request_destroy(&request);
  return result;
}
