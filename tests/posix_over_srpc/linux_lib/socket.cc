/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <sys/socket.h>

#include <cerrno>

#include "native_client/src/trusted/desc/nacl_desc_sync_socket.h"
#include "posix_over_srpc_linux.h"

NaClSrpcError nonnacl_socket(NaClSrpcChannel* channel,
                             NaClSrpcArg** ins,
                             NaClSrpcArg** outs) {
  NaClDescSyncSocket* desc;
  int fd = socket(ins[0]->u.ival, ins[1]->u.ival, ins[2]->u.ival);
  desc = new NaClDescSyncSocket;
  if ((fd < 0) || (0 == NaClDescSyncSocketCtor(desc, fd))) {
    outs[0]->u.hval = DescFactory(channel)->MakeInvalid()->desc();
    outs[2]->u.ival = errno;
  } else {
    outs[0]->u.hval = reinterpret_cast<NaClDesc*>(desc);
    outs[1]->u.ival = NewHandle(channel, fd);
  }
  return NACL_SRPC_RESULT_OK;
}
