/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <unistd.h>

#include <cerrno>

#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "posix_over_srpc_linux.h"

NaClSrpcError nonnacl_pipe(NaClSrpcChannel* channel,
                           NaClSrpcArg** ins,
                           NaClSrpcArg** outs) {
  UNREFERENCED_PARAMETER(ins);
  CloseUnusedDescs(channel);
  int pair[2];
  outs[0]->u.ival = pipe(pair);
  if (0 == outs[0]->u.ival) {
    NaClHostDesc* host_desc_pair[2];
    host_desc_pair[0] = NaClHostDescPosixMake(pair[0], NACL_ABI_O_RDONLY);
    host_desc_pair[1] = NaClHostDescPosixMake(pair[1], NACL_ABI_O_WRONLY);
    NaClDescIoDesc* io_desc = NaClDescIoDescMake(host_desc_pair[0]);
    outs[2]->u.hval = reinterpret_cast<NaClDesc*>(io_desc);
    CollectDesc(channel, io_desc, IO_DESC);
    io_desc = NaClDescIoDescMake(host_desc_pair[1]);
    outs[3]->u.hval = reinterpret_cast<NaClDesc*>(io_desc);
    CollectDesc(channel, io_desc, IO_DESC);
  } else {
    outs[2]->u.hval = DescFactory(channel)->MakeInvalid()->desc();
    outs[3]->u.hval = DescFactory(channel)->MakeInvalid()->desc();
    outs[1]->u.ival = errno;
  }
  return NACL_SRPC_RESULT_OK;
}
