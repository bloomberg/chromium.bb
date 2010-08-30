/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "posix_over_srpc.h"

struct dirent* nacl_readdir(DIR *dir) {
  __psrpc_request_t request;
  if (0 != __psrpc_request_create(&request, PSRPC_READDIR)) {
    return NULL;
  }

  request.args[0][0]->u.ival = (int)dir;

  __psrpc_make_call(&request);

  struct dirent* result = NULL;
  if (0 == request.args[1][4]->u.ival) {
    errno = request.args[1][5]->u.ival;
  } else {
    result = malloc(sizeof(*result));
    result->d_ino = request.args[1][0]->u.ival;
    result->d_off = request.args[1][1]->u.ival;
    result->d_reclen = request.args[1][2]->u.ival;
    strncpy(result->d_name, request.args[1][3]->u.sval, MAXNAMLEN);
  }

  __psrpc_request_destroy(&request);
  return result;
}
