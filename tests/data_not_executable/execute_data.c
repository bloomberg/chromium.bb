/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdint.h>
#include <stdio.h>


/*
 * TODO(mseaborn): We should be able to use the following, but it
 * gives an internal compiler error on x86-64 nacl-gcc:
 *   unsigned char code[] __attribute__((aligned(32))) = { 0xc3 /\* ret *\/ };
 */

/*
 * We put the buffer in the data segment (low address) rather than on
 * the stack (high address) so that the address is not affected by the
 * ARM sandbox's 0xf000000f address masking.  However, ARM's mask
 * should be relaxed -- see
 * http://code.google.com/p/nativeclient/issues/detail?id=462.
 * -- and then this comment can be removed (TODO(mseaborn)).
 */
uint8_t buf[64];

int main() {
  void (*func)();
  /* Round up to be bundle-aligned. */
  uintptr_t code_ptr = ((uintptr_t) buf + 31) & ~31;
#if defined(__i386__) || defined(__x86_64__)
  *(uint8_t *) code_ptr = 0xc3; /* RET */
#elif defined(__arm__)
  *(uint32_t *) code_ptr = 0xe12fff1e; /* BX LR */
#else
# error Unknown architecture
#endif

  /* Double cast required to stop gcc complaining. */
  func = (void (*)()) (uintptr_t) code_ptr;

  fprintf(stdout, "This should fault...\n");
  fflush(stdout);
  func();
  fprintf(stdout, "We're still running. This is bad.\n");
  return 1;
}
