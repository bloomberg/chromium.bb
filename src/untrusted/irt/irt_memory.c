/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdint.h>

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

static int nacl_irt_sysbrk(void **newbrk) {
  /*
   * The syscall does not actually indicate error.  It just returns the
   * new current value, which is unchanged if something went wrong.
   * But if the requested value was below the end of the data segment,
   * the new value will be greater, but this is not "going wrong".
   * Here we just approximate a saner interface: you get what you requested,
   * you did a "probe" request passing NULL in, or it's an error.
   * TODO(mcgrathr): this interface should just go away!!
   */
  void *requested = *newbrk;
  void *got = NACL_SYSCALL(brk)(requested);

  if (got == requested || requested == NULL) {
    *newbrk = got;
    return 0;
  }

  return ENOMEM;
}

static int nacl_irt_mmap(void **addr, size_t len,
                         int prot, int flags, int fd, off_t off) {
  uint32_t rv = (uintptr_t) NACL_SYSCALL(mmap)(*addr, len, prot, flags,
                                               fd, &off);
  if ((uint32_t) rv > 0xffff0000u)
    return -(int32_t) rv;
  *addr = (void *) (uintptr_t) rv;
  return 0;
}

/*
 * mmap from nacl-irt-memory-0.1 interface should ignore PROT_EXEC bit for
 * backward-compatibility reasons.
 */
static int nacl_irt_mmap_v0_1(void **addr, size_t len,
                              int prot, int flags, int fd, off_t off) {
  return nacl_irt_mmap(addr, len, prot & ~PROT_EXEC, flags, fd, off);
}

static int nacl_irt_munmap(void *addr, size_t len) {
  return -NACL_SYSCALL(munmap)(addr, len);
}

static int nacl_irt_mprotect(void *addr, size_t len, int prot) {
  return -NACL_SYSCALL(mprotect)(addr, len, prot);
}

const struct nacl_irt_memory_v0_1 nacl_irt_memory_v0_1 = {
  nacl_irt_sysbrk,
  nacl_irt_mmap_v0_1,
  nacl_irt_munmap,
};

const struct nacl_irt_memory_v0_2 nacl_irt_memory_v0_2 = {
  nacl_irt_sysbrk,
  nacl_irt_mmap,
  nacl_irt_munmap,
  nacl_irt_mprotect,
};

const struct nacl_irt_memory nacl_irt_memory = {
  nacl_irt_mmap,
  nacl_irt_munmap,
  nacl_irt_mprotect,
};
