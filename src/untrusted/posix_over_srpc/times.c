/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <errno.h>
#include <sys/times.h>

#include "native_client/src/untrusted/posix_over_srpc/posix_over_srpc.h"

clock_t times(struct tms *buffer) {
  __psrpc_request_t request;
  if (0 != __psrpc_request_create(&request, PSRPC_TIMES)) {
    return (clock_t)-1;
  }

  __psrpc_make_call(&request);

  clock_t result = (clock_t)request.args[1][2]->u.ival;
  if (result == (clock_t)-1) {
    errno = request.args[1][3]->u.ival;
  } else {
    buffer->tms_utime = request.args[1][0]->u.ival;
    buffer->tms_stime = request.args[1][1]->u.ival;
  }

  __psrpc_request_destroy(&request);
  return result;
}
