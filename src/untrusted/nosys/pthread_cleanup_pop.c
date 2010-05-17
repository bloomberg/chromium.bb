/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/untrusted/nosys/warning.h"
#include "native_client/src/untrusted/pthread/pthread.h"

void pthread_cleanup_pop(int execute) {
  /* Unimplemented */
}

stub_warning(pthread_cleanup_pop);
