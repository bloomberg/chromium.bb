/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <unistd.h>

#include <cerrno>

#include "native_client/tests/posix_over_srpc/linux_lib/posix_over_srpc_linux.h"

NaClSrpcError nonnacl_pathconf(NaClSrpcChannel* channel,
                               NaClSrpcArg** ins,
                               NaClSrpcArg** outs) {
  UNREFERENCED_PARAMETER(channel);
  outs[0]->u.ival = pathconf(ins[0]->u.sval, ins[1]->u.ival);
  outs[1]->u.ival = errno;
  return NACL_SRPC_RESULT_OK;
}
