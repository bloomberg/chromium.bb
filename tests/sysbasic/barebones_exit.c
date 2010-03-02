/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl test for super simple program not using newlib
 */

/*
 * these were lifted from src/trusted/service_runtime/nacl_config.h
 * NOTE: we cannot include this file here
 * TODO(robertm): make this file available in the sdk
 */

#define NACL_INSTR_BLOCK_SHIFT         5
#define NACL_PAGESHIFT                12
#define NACL_SYSCALL_START_ADDR       (16 << NACL_PAGESHIFT)
#define NACL_SYSCALL_ADDR(syscall_number)                               \
     (NACL_SYSCALL_START_ADDR + (syscall_number << NACL_INSTR_BLOCK_SHIFT))

#define NACL_SYSCALL(s) ((TYPE_nacl_ ## s) NACL_SYSCALL_ADDR(NACL_sys_ ## s))

typedef void (*TYPE_nacl_exit) (int status);

#include <bits/nacl_syscalls.h>


int main() {
  NACL_SYSCALL(exit)(42);
  return 0;
}
