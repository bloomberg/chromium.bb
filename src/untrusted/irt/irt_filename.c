/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/src/untrusted/irt/irt_dev.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

static int nacl_irt_open(const char *pathname, int oflag, mode_t cmode,
                         int *newfd) {
  int rv = NACL_GC_WRAP_SYSCALL(NACL_SYSCALL(open)(pathname, oflag, cmode));
  if (rv < 0)
    return -rv;
  *newfd = rv;
  return 0;
}

static int nacl_irt_stat(const char *pathname, struct stat *st) {
  return -NACL_SYSCALL(stat)(pathname, st);
}

static int nacl_irt_mkdir(const char *pathname, mode_t mode) {
  return -NACL_SYSCALL(mkdir)(pathname, mode);
}

static int nacl_irt_rmdir(const char *pathname) {
  return -NACL_SYSCALL(rmdir)(pathname);
}

static int nacl_irt_chdir(const char *pathname) {
  return -NACL_SYSCALL(chdir)(pathname);
}

static int nacl_irt_getcwd(char *pathname, size_t len) {
  return -NACL_SYSCALL(getcwd)(pathname, len);
}

static int nacl_irt_unlink(const char *pathname) {
  return -NACL_SYSCALL(unlink)(pathname);
}

const struct nacl_irt_filename nacl_irt_filename = {
  nacl_irt_open,
  nacl_irt_stat,
};

const struct nacl_irt_dev_filename_v0_2 nacl_irt_dev_filename = {
  nacl_irt_open,
  nacl_irt_stat,
  nacl_irt_mkdir,
  nacl_irt_rmdir,
  nacl_irt_chdir,
  nacl_irt_getcwd,
  nacl_irt_unlink,
};
