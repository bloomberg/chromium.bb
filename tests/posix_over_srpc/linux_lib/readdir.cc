/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <dirent.h>

#include <cerrno>
#include <cstring>

#include "posix_over_srpc_linux.h"

NaClSrpcError nonnacl_readdir(NaClSrpcChannel* channel,
                              NaClSrpcArg** ins,
                              NaClSrpcArg** outs) {
  UNREFERENCED_PARAMETER(channel);
  DIR* dir;
  GET_VALID_PTR(ins[0]->u.ival, &dir);
  dirent* result = readdir(dir);
  if (NULL != result) {
    outs[0]->u.ival = result->d_ino;
    outs[1]->u.ival = result->d_off;
    outs[2]->u.ival = result->d_reclen;
    outs[3]->u.sval = strdup(result->d_name);
    outs[4]->u.ival = 1;
  } else {
    outs[3]->u.sval = strdup("");
    outs[4]->u.ival = 0;
    outs[5]->u.ival = errno;
  }
  return NACL_SRPC_RESULT_OK;
}
