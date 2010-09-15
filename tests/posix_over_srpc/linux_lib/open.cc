/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <fcntl.h>
#include <sys/stat.h>

#include <cerrno>

#include "native_client/tests/posix_over_srpc/linux_lib/posix_over_srpc_linux.h"

NaClSrpcError nonnacl_open(NaClSrpcChannel* channel,
                           NaClSrpcArg** ins,
                           NaClSrpcArg** outs) {
  CloseUnusedDescs(channel);
  nacl::DescWrapper* desc_wr;
  int mode = ins[2]->u.ival;
  // TODO(mikhailt): Remove this mode hack.
  mode = S_IRUSR | S_IWUSR;
  desc_wr = DescFactory(channel)->OpenHostFile(ins[0]->u.sval, ins[1]->u.ival,
                                               mode);
  if (NULL == desc_wr) {
    outs[0]->u.hval = DescFactory(channel)->MakeInvalid()->desc();
    outs[1]->u.ival = errno;
  } else {
    outs[0]->u.hval = desc_wr->desc();
    CollectDesc(channel, desc_wr, DESC_WRAPPER);
  }
  return NACL_SRPC_RESULT_OK;
}
