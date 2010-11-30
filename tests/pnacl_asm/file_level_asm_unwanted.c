/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * This is a simple test to detect that the developer does not include
 * unwanted non-portable asm in LLVM bitcode.
 */

/* Testing: File level asm */

#if defined(TARGET_FULLARCH_x86_32)
/* Module level ASM */
__asm__ ("mov  %eax, %eax");
#endif

#if defined(TARGET_FULLARCH_x86_64)
/* Module level ASM */
__asm__ ("mov  %rax, %rax");
#endif

#if defined(TARGET_FULLARCH_arm)
/* Module level ASM */
__asm__ ("mov  r0, r0");
#endif

int main(int argc, char *argv[]) {
  return 0;
}
