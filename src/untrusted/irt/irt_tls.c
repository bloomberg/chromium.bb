/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

static int nacl_irt_tls_init(void *thread_ptr) {
  /* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
#if defined(__i386__)
  /*
   * Sanity check for ensuring that %gs:0 will work on x86-32.  See
   * the comment for the same check in nacl_irt_thread_create().
   */
  if (*(void **) thread_ptr != thread_ptr)
    return EINVAL;
#endif

  return -NACL_SYSCALL(tls_init)(thread_ptr);
}

static void *nacl_irt_tls_get(void) {
  return NACL_SYSCALL(tls_get)();
}

const struct nacl_irt_tls nacl_irt_tls = {
  nacl_irt_tls_init,
  nacl_irt_tls_get,
};
