/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/nacl_irt.h"

/*
 * NaCl low-level sbrk implementation.  We don't provide brk.
 * This is not thread-safe in traditional Unix or modern Linux,
 * so ours isn't either.
 */

static void *curbrk;

void *sbrk(intptr_t increment) {
  void *old_break;
  void *new_break;
  int error;

  if (NULL == curbrk) {
    error = __libnacl_irt_memory.sysbrk(&curbrk);
    if (error) {
      errno = error;
      return (void *) -1L;
    }
  }

  old_break = curbrk;
  new_break = (char *) old_break + increment;

  error = __libnacl_irt_memory.sysbrk(&new_break);
  if (error) {
    errno = error;
    return (void *) -1L;
  }

  curbrk = new_break;
  return old_break;
}
