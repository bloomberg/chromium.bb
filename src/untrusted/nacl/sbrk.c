/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdint.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

/*
 * NaCl low-level brk and sbrk implementation.
 * TODO(sehr,bsy): figure out threading issues and how to provide a thread-safe
 * version of this.
 */

/*
 * TODO(sehr): this should probably be static.
 */
void *__nacl_break = 0;

int __NaClBrk(void  *end_data_segment) {
  void    *old_break;
  void    *ret;

  if (0 == __nacl_break) {
    __nacl_break = NACL_SYSCALL(sysbrk)(0);
  }

  if (end_data_segment == __nacl_break)
    return 0;
  old_break = __nacl_break;
  ret = NACL_SYSCALL(sysbrk)(end_data_segment);
  if (ret == old_break) return -1;
  __nacl_break = ret;
  return 0;
}

void  *sbrk(intptr_t increment) {
  void  *old_break;
  int   ret;

  if (0 == __nacl_break) {
    __nacl_break = NACL_SYSCALL(sysbrk)(0);
  }

  old_break = __nacl_break;

  ret = __NaClBrk((char*)__nacl_break + increment);
  if (ret == -1) {
    errno = ENOMEM;
    return (void *) -1;
  }

  return old_break;
}
