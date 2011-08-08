/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

static int nacl_irt_close(int fd) {
  return -NACL_SYSCALL(close)(fd);
}

static int nacl_irt_read(int fd, void *buf, size_t count, size_t *nread) {
  int rv = NACL_GC_WRAP_SYSCALL(NACL_SYSCALL(read)(fd, buf, count));
  if (rv < 0)
    return -rv;
  *nread = rv;
  return 0;
}

static int nacl_irt_write(int fd, const void *buf, size_t count,
                          size_t *nwrote) {
  int rv = NACL_GC_WRAP_SYSCALL(NACL_SYSCALL(write)(fd, buf, count));
  if (rv < 0)
    return -rv;
  *nwrote = rv;
  return 0;
}

static int nacl_irt_seek(int fd, off_t offset, int whence, off_t *new_offset) {
  int rv = NACL_SYSCALL(lseek)(fd, &offset, whence);
  if (rv < 0)
    return -rv;
  *new_offset = offset;
  return 0;
}

static int nacl_irt_dup(int fd, int *newfd) {
  int rv = NACL_SYSCALL(dup)(fd);
  if (rv < 0)
    return -rv;
  *newfd = rv;
  return 0;
}

static int nacl_irt_dup2(int fd, int newfd) {
  int rv = NACL_SYSCALL(dup2)(fd, newfd);
  if (rv < 0)
    return -rv;
  return 0;
}

static int nacl_irt_fstat(int fd, struct stat *st) {
  return -NACL_SYSCALL(fstat)(fd, st);
}

static int nacl_irt_getdents(int fd, struct dirent *buf, size_t count,
                             size_t *nread) {
  int rv = NACL_GC_WRAP_SYSCALL(NACL_SYSCALL(getdents)(fd, buf, count));
  if (rv < 0)
    return -rv;
  *nread = rv;
  return 0;
}

const struct nacl_irt_fdio nacl_irt_fdio = {
  nacl_irt_close,
  nacl_irt_dup,
  nacl_irt_dup2,
  nacl_irt_read,
  nacl_irt_write,
  nacl_irt_seek,
  nacl_irt_fstat,
  nacl_irt_getdents,
};
