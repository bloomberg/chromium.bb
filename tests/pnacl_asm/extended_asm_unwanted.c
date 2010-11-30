/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * This is a simple test to detect that the developer does not include
 * unwanted non-portable asm in LLVM bitcode.
 */

/* Testing extended asm */

#include <stdio.h>

#if defined(TARGET_FULLARCH_x86_32)
int extended_asm_x86_32() {
  int x = 11, five_times_x;
  asm ("leal (%1,%1,4), %0\n\t"
       : "=r" (five_times_x)
       : "r" (x)
       );
  return five_times_x;
}
#endif

#if defined(TARGET_FULLARCH_x86_64)
int extended_asm_x86_64() {
  long long x = 11, five_times_x;
  asm ("leaq (%1,%1,4), %0\n\t"
       : "=r" (five_times_x)
       : "r" (x)
       );
  return (int) five_times_x;
}
#endif

#if defined(TARGET_FULLARCH_arm)
int extended_asm_arm() {
  int x = 11, five_times_x;
  asm ("add %1,%1,%0, lsl #2\n\t"
       : "=r" (five_times_x)
       : "r" (x)
       );
  return five_times_x;
}
#endif

int main(int argc, char *argv[]) {
  int ret_val = 0;
  #if defined(TARGET_FULLARCH_x86_32)
  ret_val = extended_asm_x86_32();
  #endif

  #if defined(TARGET_FULLARCH_x86_64)
  ret_val = extended_asm_x86_64();
  #endif

  #if defined(TARGET_FULLARCH_arm)
  ret_val = extended_asm_arm();
  #endif

  printf("returning: %d\n", ret_val);
  return ret_val;
}
