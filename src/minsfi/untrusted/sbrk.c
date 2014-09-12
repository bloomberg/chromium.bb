/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/minsfi_syscalls.h"

static char *program_break = NULL, *heap_upper_limit;

void *sbrk(intptr_t increment) {
  /* If this is the first time sbrk() is invoked, query the heap limits. */
  if (program_break == NULL)
    __minsfi_syscall_heaplim(&program_break, &heap_upper_limit);

  void *prev_brk = program_break;
  if (increment > 0) {
    uint32_t free_mem = heap_upper_limit - program_break;
    if (increment <= free_mem) {
      program_break += increment;
    } else {
      errno = ENOMEM;
      return (void*) -1;
    }
  } else if (increment < 0) {
    /*  is an undefined operation. */
    __builtin_trap();
  }

  return prev_brk;
}
