/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <dirent.h>

#include <cerrno>

#include "posix_over_srpc_linux.h"

NaClSrpcError nonnacl_closedir(NaClSrpcChannel* channel,
                               NaClSrpcArg** ins,
                               NaClSrpcArg** outs) {
  UNREFERENCED_PARAMETER(channel);
  DIR* dir;
  GET_VALID_PTR(ins[0]->u.ival, &dir);
  outs[0]->u.ival = closedir(dir);
  outs[1]->u.ival = errno;
  return NACL_SRPC_RESULT_OK;
}
