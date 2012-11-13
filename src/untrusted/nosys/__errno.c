/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Stub simple non-thread safe routine for `__errno' for porting support.
 */

extern int errno;

int *__errno(void) {
  return &errno;
}

#include "native_client/src/untrusted/nosys/warning.h"
link_warning(__errno,
  "the stub `__errno\' is not thread-safe, don\'t use it in multi-threaded environment");
