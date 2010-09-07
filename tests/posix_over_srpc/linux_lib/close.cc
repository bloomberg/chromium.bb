/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <unistd.h>

#include "posix_over_srpc_linux.h"

NaClSrpcError nonnacl_close(NaClSrpcChannel* channel,
                            NaClSrpcArg** ins,
                            NaClSrpcArg** outs) {
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(outs);
  int fd;
  GET_VALID_FD(ins[0]->u.ival, &fd);
  close(fd);
  return NACL_SRPC_RESULT_OK;
}
