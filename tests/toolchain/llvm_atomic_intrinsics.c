/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This test ensures that the (pnacl) backends can deal with atomic intrinsics
 * Note: this is more of a syntactical check as we do not run this
 * multithreaded.
 * Some real testing is done here:  tests/threads/thread_test.c
 *
 * More info can be found here:
 *   http://llvm.org/docs/LangRef.html#i_atomicrmw
 *   http://gcc.gnu.org/onlinedocs/gcc-4.6.3/gcc/Atomic-Builtins.html#Atomic-Builtins
 *   Note, some more changes ahead:
 *   http://gcc.gnu.org/onlinedocs/gcc-4.7.1/gcc/_005f_005fatomic-Builtins.html
 *
 *   NOTE: clang only
 *   __sync_swap(addr, val)
 *   lowers to: atomicrmw xchg i32* %addr, i32 %val seq_cst
 *
 *  __sync_fetch_and_add(addr, val);
 *  lowers to: atomicrmw add i32* %addr, i32 val seq_cst
 *
 *  __sync_fetch_and_sub(addr, val);
 *  lowers to: atomicrmw sub i32* %addr, i32 2 seq_cst
 *
 *  __sync_fetch_and_or(addr, val);
 *  lowers to: atomicrmw or i32* %addr, i32 %val seq_cst
 *
 *  __sync_fetch_and_and(addr, val);
 *  lowers to: atomicrmw and i32* %addr, i32 %val seq_cst
 *
 * NOTE: http://llvm.org/bugs/show_bug.cgi?id=8842
 *  __sync_fetch_and_nand(addr, val);
 *  lowers to: atomicrmw or i32* %addr, i32 %val seq_cst
 *
 *  __sync_fetch_and_xor(addr, val);
 *  lowers to: atomicrmw xor i32* %addr, i32 %val seq_cst
 *
 *  __sync_fetch_and_min(addr, val);
 *  lowers to: atomicrmw min i32* %addr, i32 %val seq_cst
 *
 *  __sync_fetch_and_max(addr, val)
 *  lowers to: atomicrmw max i32* %addr, i32 %val seq_cst
 *
 *  __sync_fetch_and_umin(addr, val)
 *  lowers to: atomicrmw umin i32* %addr, i32 %val seq_cst
 *
 *  __sync_fetch_and_umax(addr, val)
 *  lowers to: atomicrmw umax i32* %addr, i32 %val seq_cst
 *
 *  TODO(robertm): add tests for __sync_OP_and_fetch
 *  TODO(robertm): add tests for cmpxchg, store atomic, fence and other stuff
 *                 mentioned in the llvm documents
 *  TODO(robertm): test other datawidths besides uint32_t
 *  TODO(robertm): consider check the return value as well.
 */

#include <stdio.h>
#include <stdint.h>
#include "native_client/tests/toolchain/utils.h"

#define test_atomic32(op, var, initial, val, expected) \
  do { \
    printf("testing: 0x%x %s 0x%x == (expected 0x%x)\n",    \
           initial, #op, val, expected); \
    var = initial; \
    printf("old val: 0x%x\n", __sync_ ## op(&var, val));      \
    if (var != expected) { \
      printf("ERROR: result 0x%x", var); \
      exit(1); \
    } \
  } while(0)

/* declared volatile to confuse the optimizer */
volatile int fiftyfive = 55;

int main(int argc, char* argv[]) {
  uint32_t uvar;

#ifdef __clang__
  test_atomic32(swap, uvar, fiftyfive, 1 , 1);
#endif
  test_atomic32(fetch_and_add, uvar, fiftyfive, 50, 105);
  test_atomic32(fetch_and_sub, uvar, fiftyfive, 50, 5);
  test_atomic32(fetch_and_xor, uvar, fiftyfive, 50, 5);
  test_atomic32(fetch_and_or, uvar, fiftyfive, 9, 63);
  test_atomic32(fetch_and_and, uvar, fiftyfive, 5, 5);
  /* "nand" is not C99 - also see http://llvm.org/bugs/show_bug.cgi?id=8842 */

  /* these work for pnacl x86-32/64 but not for arm or nacl-gcc */
#if 0
  int32_t svar;
  test_atomic32(fetch_and_umin, uvar, fiftyfive, 50, 50);
  test_atomic32(fetch_and_umax, uvar, fiftyfive, 500, 500);

  test_atomic32(fetch_and_min, svar, fiftyfive, 50, 50);
  test_atomic32(fetch_and_max, svar, fiftyfive, 500, 500);
#endif
  return fiftyfive;
}
