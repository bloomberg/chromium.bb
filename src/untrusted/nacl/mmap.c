/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Wrapper for syscall.
 */

#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/nacl_syscalls.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

void *mmap(void *start, size_t length, int prot, int flags,
           int desc, off_t offset) {
  void *retval = NACL_SYSCALL(mmap)(start, length, prot, flags, desc, offset);
  if ((uint32_t) retval > 0xffff0000u) {
    /*
     * __nacl_mmap returns an address as its value if it succeeds and
     * a negated errno value if it fails.  Differentiating between the
     * two requires testing that the address is not "valid", which we mean
     * to be is in (-4k, -1).
     */
    errno = -(int) retval;
    return (void *) -1;
  }
  return retval;
}
