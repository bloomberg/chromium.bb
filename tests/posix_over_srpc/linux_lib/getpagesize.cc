/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <unistd.h>

#include "posix_over_srpc_linux.h"

NaClSrpcError nonnacl_getpagesize(NaClSrpcChannel* channel,
                                  NaClSrpcArg** ins,
                                  NaClSrpcArg** outs) {
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(ins);
  outs[0]->u.ival = getpagesize();
  return NACL_SRPC_RESULT_OK;
}
