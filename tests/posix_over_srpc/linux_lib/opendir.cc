/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <dirent.h>

#include <cerrno>

#include "native_client/tests/posix_over_srpc/linux_lib/posix_over_srpc_linux.h"

NaClSrpcError nonnacl_opendir(NaClSrpcChannel* channel,
                              NaClSrpcArg** ins,
                              NaClSrpcArg** outs) {
  DIR* dir = opendir(ins[0]->u.sval);
  if (NULL == dir) {
    outs[0]->u.ival = 0;
    outs[1]->u.ival = errno;
  } else {
    outs[0]->u.ival = NewHandle(channel, dir);
  }
  return NACL_SRPC_RESULT_OK;
}
