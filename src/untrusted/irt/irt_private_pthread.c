/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <pthread.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/tls.h"

/*
 * This is used by the pthread mutex implementation.  It really doesn't
 * need a pthread_t it can do anything with, just a unique identifier for
 * the thread.  So using our TLS pointer works just fine.
 */
pthread_t pthread_self(void) {
  return __nacl_read_tp();
}

/*
 * libstdc++ makes minimal use of pthread_key_t stuff.
 */

#undef  PTHREAD_KEYS_MAX
#define PTHREAD_KEYS_MAX        2

#define NC_TSD_NO_MORE_KEYS     irt_tsd_no_more_keys()
static void irt_tsd_no_more_keys(void) {
  static const char msg[] = "IRT: too many pthread keys\n";
  write(2, msg, sizeof msg - 1);
}

#include "native_client/src/untrusted/pthread/nc_tsd.c"
