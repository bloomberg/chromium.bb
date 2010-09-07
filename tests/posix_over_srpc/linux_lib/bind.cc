/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <sys/socket.h>

#include <cerrno>
#include <cstring>

#include "posix_over_srpc_linux.h"

NaClSrpcError nonnacl_bind(NaClSrpcChannel* channel,
                           NaClSrpcArg** ins,
                           NaClSrpcArg** outs) {
  UNREFERENCED_PARAMETER(channel);
  int fd;
  GET_VALID_FD(ins[0]->u.ival, &fd);
  struct sockaddr address;
  address.sa_family = ins[1]->u.ival;
  memcpy(address.sa_data, ins[2]->u.caval.carr, ins[2]->u.caval.count);
  outs[0]->u.ival = bind(fd, &address, ins[3]->u.ival);
  if (0 != outs[0]->u.ival) {
    outs[1]->u.ival = errno;
  }
  return NACL_SRPC_RESULT_OK;
}

