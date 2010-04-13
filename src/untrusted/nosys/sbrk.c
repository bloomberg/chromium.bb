/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub simple non-thread safe routine for `sbrk' for porting support.
 */

#include <unistd.h>

extern char end; /* Supplied by linker */

static char *sbk_limit;

void *sbrk(intptr_t increment) {
  char *sbrk_old_limit = sbk_limit ? sbk_limit : &end;

  sbk_limit += increment;

  return sbrk_old_limit;
}

#include "native_client/src/untrusted/nosys/warning.h"
link_warning(errno,
  "the stub `sbrk\' is not thread-safe, don\'t use it in multi-threaded environment");
