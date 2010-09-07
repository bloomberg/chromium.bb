/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <sys/socket.h>

#include <cerrno>

#include "native_client/src/trusted/desc/nacl_desc_sync_socket.h"
#include "posix_over_srpc_linux.h"

NaClSrpcError nonnacl_accept(NaClSrpcChannel* channel,
                             NaClSrpcArg** ins,
                             NaClSrpcArg** outs) {
  NaClDescSyncSocket* desc;
  int fd;
  GET_VALID_FD(ins[0]->u.ival, &fd);
  int new_fd = accept(fd, NULL, 0);
  desc = new NaClDescSyncSocket;
  if ((new_fd < 0) || (0 == NaClDescSyncSocketCtor(desc, new_fd))) {
    outs[0]->u.hval = DescFactory(channel)->MakeInvalid()->desc();
    outs[2]->u.ival = errno;
  } else {
    outs[0]->u.hval = reinterpret_cast<NaClDesc*>(desc);
    outs[1]->u.ival = NewHandle(channel, new_fd);
  }
  return NACL_SRPC_RESULT_OK;
}
