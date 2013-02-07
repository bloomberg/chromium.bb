/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Non-IRT definition of sbrk for use only in private tests.
 */

#include <errno.h>
#include <unistd.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

static void *current_break;

void *sbrk(intptr_t increment) {
  void *old_break;
  void *new_break;

  if (0 == current_break)
    current_break = NACL_SYSCALL(brk)(0);

  old_break = current_break;
  new_break = old_break + increment;

  if (new_break != old_break) {
    new_break = NACL_SYSCALL(brk)(new_break);
    if (new_break == old_break) {
      errno = ENOMEM;
      return (void *) -1;
    }
    current_break = new_break;
  }

  return old_break;
}
