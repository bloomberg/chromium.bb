/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <unistd.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

static int IrtInit(void) {
  static int initialized = 0;
  if (initialized) {
    return 0;
  }
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  NaClLogModuleInit();  /* Enable NaClLog'ing used by CHECK(). */
  initialized = 1;
  return 0;
}

static __attribute__((constructor)) void CallIrtInit(void) {
  if (IrtInit()) {
    static const char fatal_msg[] =
        "irt initialization (IRTInit) failed.\n";
    write(2, fatal_msg, sizeof(fatal_msg) - 1);
    _exit(-1);
  }
}
