/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <sys/times.h>

#include <cerrno>

#include "native_client/tests/posix_over_srpc/linux_lib/posix_over_srpc_linux.h"

NaClSrpcError nonnacl_times(NaClSrpcChannel* channel,
                            NaClSrpcArg** ins,
                            NaClSrpcArg** outs) {
  UNREFERENCED_PARAMETER(channel);
  UNREFERENCED_PARAMETER(ins);
  struct tms t;
  outs[2]->u.ival = times(&t);
  outs[0]->u.ival = t.tms_utime;
  outs[1]->u.ival = t.tms_stime;
  // TODO(mikhailt): Add tms_cutime and tms_cstime fields realization.
  outs[3]->u.ival = errno;
  return NACL_SRPC_RESULT_OK;
}
