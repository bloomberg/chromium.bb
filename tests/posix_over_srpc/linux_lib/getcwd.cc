/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <unistd.h>

#include <cerrno>

#include "posix_over_srpc_linux.h"

NaClSrpcError nonnacl_getcwd(NaClSrpcChannel* channel,
                             NaClSrpcArg** ins,
                             NaClSrpcArg** outs) {
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(ins);
  if (NULL == getcwd(outs[0]->u.caval.carr, outs[0]->u.caval.count)) {
    outs[1]->u.ival = 0;
    outs[2]->u.ival = errno;
  } else {
    outs[1]->u.ival = 1;
  }
  return NACL_SRPC_RESULT_OK;
}
