/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <sys/socket.h>

#include <cerrno>

#include "native_client/tests/posix_over_srpc/linux_lib/posix_over_srpc_linux.h"

NaClSrpcError nonnacl_listen(NaClSrpcChannel* channel,
                             NaClSrpcArg** ins,
                             NaClSrpcArg** outs) {
  UNREFERENCED_PARAMETER(channel);
  int fd;
  GET_VALID_FD(ins[0]->u.ival, &fd);
  outs[0]->u.ival = listen(fd, ins[1]->u.ival);
  if (0 != outs[0]->u.ival) {
    outs[1]->u.ival = errno;
  }
  return NACL_SRPC_RESULT_OK;
}

