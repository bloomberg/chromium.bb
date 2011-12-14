/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This test ensures that the backends can deal with bit manipulation
 * intrinsics.
 */

#include <stdio.h>
#include <stdint.h>
#include "native_client/tests/toolchain/utils.h"

/*  __builtin_popcount(x) */
uint32_t llvm_intrinsic_ctpop(uint32_t) __asm__("llvm.ctpop.i32");
uint64_t llvm_intrinsic_ctpopll(uint64_t) __asm__("llvm.ctpop.i64");

/* __builtin_clz(x) */
uint32_t llvm_intrinsic_ctlz(uint32_t, _Bool) __asm__("llvm.ctlz.i32");
uint64_t llvm_intrinsic_ctlzll(uint64_t, _Bool) __asm__("llvm.ctlz.i64");

/* __builtin_ctz(x) */
uint32_t llvm_intrinsic_cttz(uint32_t, _Bool) __asm__("llvm.cttz.i32");
uint64_t llvm_intrinsic_cttzll(uint64_t, _Bool) __asm__("llvm.cttz.i64");

/* __builtin_bswap32(x) */
uint32_t llvm_intrinsic_bswap(uint32_t) __asm__("llvm.bswap.i32");
uint64_t llvm_intrinsic_bswapll(uint64_t) __asm__("llvm.bswap.i64");

/* volatile prevents partial evaluation by llvm */
volatile uint32_t i32[] = {0xaa, 0xaa00};
volatile uint64_t i64[] = {0xaa000000ll, 0xaa00000000ll};

#define print_op(op, x) \
  printf("%s: %u\n", #op, (unsigned) llvm_intrinsic_ ## op(x))

#define print_op2(op, x, y) \
  printf("%s: %u\n", #op, (unsigned) llvm_intrinsic_ ## op(x, y))

int main(int argc, char* argv[]) {
  int i;
  for (i = 0; i < ARRAY_SIZE_UNSAFE(i32); ++i) {
    printf("\ni32 value is: %08x\n", (unsigned) i32[i]);
    print_op(ctpop, i32[i]);
    print_op2(ctlz, i32[i], 1);
    print_op2(cttz, i32[i], 1);
    printf("%s: %08x\n", "bswap", (unsigned) llvm_intrinsic_bswap(i32[i]));
  }

  for (i = 0; i < ARRAY_SIZE_UNSAFE(i64); ++i) {
    printf("\ni64 value is: %016llx\n",  i64[i]);
    print_op(ctpopll, i64[i]);
    print_op2(ctlzll, i64[i], 1);
    print_op2(cttzll, i64[i], 1);
    printf("%s: %016llx\n", "bswapll", llvm_intrinsic_bswapll(i64[i]));
  }

  return 0;
}
