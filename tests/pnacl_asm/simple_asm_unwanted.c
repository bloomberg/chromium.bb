/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * This is a simple test to detect that the developer does not include
 * unwanted non-portable asm in LLVM bitcode.
 */

/* Testing: Simple inline asm */

#if defined(TARGET_FULLARCH_x86_32)
#define X86_32_ONE_LINER "mov  %eax, %eax"
void multi_liner_x86_32() {
  __asm__ volatile (X86_32_ONE_LINER "\n\t"
                    X86_32_ONE_LINER "\n\t"
                    X86_32_ONE_LINER "\n\t");
}
#endif

#if defined(TARGET_FULLARCH_x86_64)
#define X86_64_ONE_LINER "mov  %rax, %rax"
void multi_liner_x86_64() {
  __asm__ volatile (X86_64_ONE_LINER "\n\t"
                    X86_64_ONE_LINER "\n\t"
                    X86_64_ONE_LINER "\n\t");
}
#endif

#if defined(TARGET_FULLARCH_arm)
#define ARM_ONE_LINER "mov  r0, r0"
void multi_liner_arm() {
  __asm__ volatile (ARM_ONE_LINER "\n\t"
                    ARM_ONE_LINER "\n\t"
                    ARM_ONE_LINER "\n\t");
}
#endif

int main(int argc, char *argv[]) {
  #if defined(TARGET_FULLARCH_x86_32)
  multi_liner_x86_32();
  #endif

  #if defined(TARGET_FULLARCH_x86_64)
  multi_liner_x86_64();
  #endif

  #if defined(TARGET_FULLARCH_arm)
  multi_liner_arm();
  #endif
  return 0;
}
