/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* NOTE: This include must come before sys/nacl_syscalls.h to avoid */
/* a typechecking error for nanosleep. */
#include "native_client/src/untrusted/irt/irt.h"

#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
/* TODO(bradchen): resolve dissonance between nacl_syscalls.h and    */
/* prescribed Linux includes. Note that nacl_syscalls is incomplete, */
/* not declaring dup, dup2 or getpid. None of the nacl_syscalls.h    */
/* declarations work for a glibc build. */
/* Perhaps the solution should be: just #include irt.h               */
#include <sys/nacl_syscalls.h>
#include <unistd.h>

static const char *nacl_version(int *major, int *minor) {
  if (NULL != major) *major = 0;
  if (NULL != minor) *minor = 1;
  return NACLIRTv0_1;
}

/* TODO(bradchen): replace these libc wrapper functions with IRT-specific */
/* wrappers that behave properly with respect to errno and such.          */
static const struct nacl_core NaClCore = {
  nacl_version,
  clock,
  _exit,
  gettimeofday,
  nanosleep,
  sysbrk,
  close,
  dup,
  dup2,
  fstat,
  lseek,
  mmap,
  munmap,
  open,
  read,
  stat,
  write,
  nacl_dyncode_create,
  nacl_dyncode_delete,
  nacl_dyncode_modify,
};

const void *nacl_getinterface(const char *version) {
  if (strcmp(version, NACLIRTv0_1) == 0) {
    return &NaClCore;
  }
  return NULL;
}
