/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

#if defined(__GLIBC__)
#include <unistd.h>
#else
/*
 * TODO(bsy): remove when newlib toolchain catches up
 * http://code.google.com/p/nativeclient/issues/detail?id=2714
 */
#include "native_client/src/trusted/service_runtime/include/sys/unistd.h"
#endif

int main(void) {
#if defined(__GLIBC__)
  long rv = sysconf(_SC_PAGESIZE);
#else
  long rv = sysconf(NACL_ABI__SC_PAGESIZE);
#endif

  printf("%ld\n", rv);
  return rv != (1<<16);
}
