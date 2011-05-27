/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include<native_client/tests/barebones/barebones.h>

/*
 * This test expects the following defines to be provided via -D
 * on the compiler command line:
 * TEXT_SEGMENT_SIZE between 32k and 64k (divisible by 4)
 * RODATA_SEGMENT_SIZE
 * RWDATA_SEGMENT_SIZE
 * NOP_WORD
 * EXPECTED_BREAK
 */

#define ALIGN(sec, n)  __attribute__ ((__used__, section(sec), aligned(n)))

int _start() {
  void* sbreak = NACL_SYSCALL(sysbrk)(0);
  if (sbreak != (void*) EXPECTED_BREAK) {
#if 0
    /*
     * commented out as nacl-gcc gets confused and generates
     * .plt sections otherwise. Curiously pnacl-gcc has no issues.
     */

    char buffer[16];
    myhextochar(EXPECTED_BREAK, buffer);
    myprint(buffer);
    buffer[0] = '\n';  /* NOTE: avoid (rodata) string constants */
    buffer[1] = 0;
    myprint(buffer);

    myhextochar((int) sbreak, buffer);
    myprint(buffer);
    buffer[0] = '\n';
    buffer[1] = 0;
    myprint(buffer);
#endif
    NACL_SYSCALL(exit)(1);
  }
  NACL_SYSCALL(exit)(55);
  /* UNREACHABLE */
  return 0;
}

/* dummy functions to make the linker happy (for ARM and x86-64) */
void *__aeabi_read_tp() { return 0; }
void *__nacl_read_tp() { return 0; }

/* We assume that the code above starts 0x10000 and is no longer that 8kB. This
 * ensures that the text segment at this point is exactly 16kB = 0x4000
 */

int TextRoundUpTo16k[8192 / 4] ALIGN(".text", 8192) =
{ [0 ... ((8192 / 4) - 1)] = NOP_WORD };

/* what follows should be at address 0x14000 */
