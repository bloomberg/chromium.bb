/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncval_tests.c - simple unit tests for NaCl validator
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncdecode_verbose.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate.h"
#include "native_client/src/trusted/validator/x86/ncval_seg_sfi/ncvalidate_internaltypes.h"

/* Define the set of CPU features to use while validating. */
static NaClCPUFeaturesX86 g_ncval_cpu_features;

void Info(const char *fmt, ...)
{
  va_list ap;
  fprintf(stdout, "I: ");
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
}

struct NCValTestCase {
  char *name;
  char *description;

  /* Expected results: */
  int sawfailure;        /* Whether code is expected to fail validation */
  uint32_t illegalinst;  /* Expected number of disallowed instructions */
  uint32_t instructions; /* Expected number of instructions (excluding final HLT) */

  /* Input to validator: */
  uint32_t vaddr;        /* Load address (shouldn't matter) */
  const char *data_as_hex;
};

struct NCValTestCase NCValTests[] = {
  /* NOTE: Many of these tests are now in the textual testing structure in
   * native_client/src/trusted/validator_x86/testdata/32 using
   * files "test-n.hex", "test-n.ndis", "test-n.nvals", and
   * "test-n.nvals16".
   */
  {
    "test 1",
    "a first very simple test with an illegal inst.",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 9,
    /* vaddr= */ 0x80000000,
    "55 \n"                             /* push   %ebp                     */
    "89 e5 \n"                          /* mov    %esp,%ebp                */
    "83 ec 08 \n"                       /* sub    $0x8,%esp                */
    "e8 81 00 00 00 \n"                 /* call   0x86                     */
    "e8 d3 00 00 00 \n"                 /* call   0xd8                     */
    "e8 f3 04 00 00 \n"                 /* call   0x4f8                    */
    "c9 \n"                             /* leave                           */
    "c3 \n"                             /* ret                             */
    "00 00 f4 \n"
  },
  {
    "test 6",
    "test 6: 3c 25   cmp %al, $I",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 7,
    /* vaddr= */ 0x80000000,
    "3c 25 \n"                          /* cmp    $0x25,%al                */
    "90 90 90 90 90 90 f4 \n"
  },
  {
    "test 7",
    "test 7: group2, three byte move",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 8,
    /* vaddr= */ 0x80000000,
    "c1 f9 1f 89 4d e4 \n"
    "90 90 90 90 90 90 f4 \n"
  },
  {
    "test 8",
    "test 8: five byte move",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 7,
    /* vaddr= */ 0x80000000,
    "c6 44 05 d6 00 \n"                 /* movb   $0x0,-0x2a(%ebp,%eax,1)  */
    "90 90 90 90 90 90 f4 \n"
  },
  {
    "test 9",
    "test 9: seven byte control transfer, unprotected",
    /* sawfailure= */ 1, /* illegalinst= */ 0,
    /* instructions= */ 7,
    /* vaddr= */ 0x80000000,
    "ff 24 95 c8 6e 05 08 \n"           /* jmp    *0x8056ec8(,%edx,4)      */
    "90 90 90 90 90 90 f4 \n"
  },
  {
    "test 10",
    "test 10: eight byte bts instruction",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 7,
    /* vaddr= */ 0x80000000,
    "0f ab 14 85 40 fb 27 08 \n"        /* bts    %edx,0x827fb40(,%eax,4)  */
    "90 90 90 90 90 90 f4 \n"
  },
  {
    "test 11",
    "test 11: four byte move",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 7,
    /* vaddr= */ 0x80000000,
    "66 bf 08 00 \n"                    /* mov    $0x8,%di                 */
    "90 90 90 90 90 90 f4 \n"
  },
  {
    "test 12",
    "test 12: five byte movsx",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 7,
    /* vaddr= */ 0x80000000,
    "66 0f be 04 10 \n"                 /* movsbw (%eax,%edx,1),%ax        */
    "90 90 90 90 90 90 f4 \n"
  },
  /* ldmxcsr, stmxcsr */
  {
    "test 14",
    "test 14: ldmxcsr, stmxcsr",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 10,
    /* vaddr= */ 0x80000000,
    "90 0f ae 10 90 0f ae 18 \n"
    "90 90 90 90 90 90 f4 \n"
  },
  /* invalid */
  {
    "test 15",
    "test 15: invalid instruction",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 8,
    /* vaddr= */ 0x80000000,
    "90 0f ae 21 \n"
    "90 90 90 90 90 90 f4 \n"
  },
  /* lfence */
  {
    "test 16",
    "test 16: lfence",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 8,
    /* vaddr= */ 0x80000000,
    "90 0f ae ef \n"
    "90 90 90 90 90 90 f4 \n"
  },
  {
    "test 17",
    "test 17: lock cmpxchg",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 4,
    /* vaddr= */ 0x80000000,
    "f0 0f b1 8f a8 01 00 00 \n"        /* lock cmpxchg %ecx,0x1a8(%edi)   */
    "90 90 90 f4 \n"
  },
  {
    "test 18",
    "test 18: loop branch into overlapping instruction",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 3,
    /* vaddr= */ 0x80000000,
    "bb 90 40 cd 80 85 c0 e1 f8 f4 \n"
  },
  {
    "test 19",
    "test 19: aad test",
    /* sawfailure= */ 1, /* illegalinst= */ 2,
    /* instructions= */ 5,
    /* vaddr= */ 0x80000000,
    "68 8a 80 04 08 d5 b0 c3 90 bb 90 40 cd 80 f4 \n"
  },
  {
    "test 20",
    "test 20: addr16 lea",
    /* sawfailure= */ 1, /* illegalinst= */ 2,
    /* instructions= */ 5,
    /* vaddr= */ 0x80000000,
    "68 8e 80 04 08 66 67 8d 98 ff ff c3 90 bb 90 40 cd 80 f4 \n"
  },
  {
    "test 21",
    "test 21: aam",
    /* sawfailure= */ 1, /* illegalinst= */ 2,
    /* instructions= */ 4,
    /* vaddr= */ 0x80000000,
    "68 89 80 04 08 \n"                 /* push   $0x8048089               */
    "d4 b0 \n"                          /* aam    $0xffffffb0              */
    "c3 \n"                             /* ret                             */
    "bb 90 40 cd f4 \n"                 /* mov    $0xf4cd4090,%ebx         */
    "f4 \n"                             /* hlt                             */
  },
  {
    "test 22",
    "test 22: pshufw",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 4,
    /* vaddr= */ 0x80000000,
    "68 8b 80 04 08 0f 70 ca b3 c3 bb 90 40 cd 80 f4 \n"
  },
  {
    "test 23",
    "test 23: 14-byte nacljmp using eax",
    /* sawfailure= */ 1, /* illegalinst= */ 0,
    /* instructions= */ 3,
    /* vaddr= */ 0x80000000,
    "81 e0 ff ff ff ff 81 c8 00 00 00 00 ff d0 f4 \n"
  },
  {
    "test 24",
    "test 24: 5-byte nacljmp",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "83 e0 e0 ff e0 f4 \n"
  },
  {
    "test 25",
    "test 25: 0xe3 jmp",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 1,
    /* vaddr= */ 0x80000000,
    "e3 00 f4 \n"
  },
  {
    "test 26",
    "test 26: 0xe9 jmp, nop",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "e9 00 00 00 00 90 f4 \n"
  },
  {
    "test 27",
    "test 27: 0xf0 0x80 jmp, nop",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "0f 80 00 00 00 00 90 f4 \n"
  },
  {
    "test 28",
    "test 28: 0xe9 jmp",
    /* sawfailure= */ 1, /* illegalinst= */ 0,
    /* instructions= */ 1,
    /* vaddr= */ 0x80000000,
    "e9 00 00 00 00 f4 \n"
  },
  {
    "test 30",
    "test 30: addr16 lea ret",
    /* sawfailure= */ 1, /* illegalinst= */ 2,
    /* instructions= */ 3,
    /* vaddr= */ 0x80000000,
    "67 8d b4 9a 40 c3 90 f4 \n"
  },
  {
    "test 31",
    "test 31: repz movsbl",
    /* sawfailure= */ 1, /* illegalinst= */ 2,
    /* instructions= */ 3,
    /* vaddr= */ 0x80000000,
    "f3 0f be 40 d0 c3 90 f4 \n"
  },
  {
    "test 32",
    "test 32: infinite loop",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 1,
    /* vaddr= */ 0x80000000,
    "7f fe f4 \n"
  },
  {
    "test 33",
    "test 33: bad branch",
    /* sawfailure= */ 1, /* illegalinst= */ 0,
    /* instructions= */ 1,
    /* vaddr= */ 0x80000000,
    "7f fd f4 \n"
  },
  {
    "test 34",
    "test 34: bad branch",
    /* sawfailure= */ 1, /* illegalinst= */ 0,
    /* instructions= */ 1,
    /* vaddr= */ 0x80000000,
    "7f ff f4 \n"
  },
  {
    "test 35",
    "test 35: bad branch",
    /* sawfailure= */ 1, /* illegalinst= */ 0,
    /* instructions= */ 1,
    /* vaddr= */ 0x80000000,
    "7f 00 f4 \n"
  },
  {
    "test 36",
    "test 36: bad branch",
    /* sawfailure= */ 1, /* illegalinst= */ 0,
    /* instructions= */ 1,
    /* vaddr= */ 0x80000000,
    "7f 01 f4 \n"
  },
  {
    "test 37",
    "test 37: bad branch",
    /* sawfailure= */ 1, /* illegalinst= */ 0,
    /* instructions= */ 1,
    /* vaddr= */ 0x80000000,
    "7f 02 f4 \n"
  },
  {
    "test 38",
    "test 38: intc",
    /* sawfailure= */ 1, /* illegalinst= */ 8,
    /* instructions= */ 10,
    /* vaddr= */ 0x80000000,
    "66 eb 1b 31 51 3d ef cc 2f 36 48 6e 44 2e cc 14 f4 f4 \n"
  },
  {
    "test 39",
    "test 39: bad branch",
    /* sawfailure= */ 1, /* illegalinst= */ 2,
    /* instructions= */ 7,
    /* vaddr= */ 0x80000000,
    "67 8d 1d 22 a0 05 e3 7b 9c db 08 04 b1 90 ed 12 f4 f4 \n"
  },
  {
    "test 40",
    "test 40: more addr16 problems",
    /* sawfailure= */ 1, /* illegalinst= */ 2,
    /* instructions= */ 4,
    /* vaddr= */ 0x80000000,
    "67 a0 00 00 cd 80 90 90 f4 \n"
  },
  {
    "test 41",
    "test 41: the latest non-bug from hcf",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 5,
    /* vaddr= */ 0x80000000,
    "84 d4 04 53 a0 04 6a 5a 20 cc b8 48 03 2b 96 11 f4 \n"
  },
  {
    "test 42",
    "test 42: another case from hcf",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 7,
    /* vaddr= */ 0x80000000,
    "45 7f 89 58 94 04 24 1b c3 e2 6f 1a 94 87 8f 0b f4 \n"
  },
  {
    "test 43",
    "test 43: too many prefix bytes",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "66 66 66 66 00 00 90 f4 \n"
  },
  {
    "test 44",
    "test 44: palignr (SSSE3)",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "66 0f 3a 0f d0 c0 90 f4 \n"
  },
  {
    "test 45",
    "test 45: undefined inst in 3-byte opcode space",
    /* sawfailure= */ 1, /* illegalinst= */ 2,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "66 0f 39 0f d0 c0 90 f4 \n"
  },
  {
    "test 46",
    "test 46: SSE2x near miss",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "66 0f 73 00 00 90 f4 \n"
  },
  {
    "test 47",
    "test 47: SSE2x",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "66 0f 73 ff 00 90 f4 \n"
  },
  {
    "test 48",
    "test 48: SSE2x, missing required prefix byte",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "0f 73 ff 00 90 f4 \n"
  },
  {
    "test 49",
    "test 49: 3DNow example",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "0f 0f 46 01 bf 90 f4 \n"
  },
  {
    "test 50",
    "test 50: 3DNow error example 1",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "0f 0f 46 01 00 90 f4 \n"
  },
  {
    "test 51",
    "test 51: 3DNow error example 2",
    /* sawfailure= */ 1, /* illegalinst= */ 0,
    /* instructions= */ 0,
    /* vaddr= */ 0x80000000,
    "0f 0f 46 01 f4 \n"
  },
  {
    "test 52",
    "test 52: 3DNow error example 3",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "0f 0f 46 01 be 90 f4 \n"
  },
  {
    "test 53",
    "test 53: 3DNow error example 4",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "0f 0f 46 01 af 90 f4 \n"
  },
  {
    "test 54",
    "test 54: SSE4",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "66 0f 3a 0e d0 c0 90 f4 \n"
  },
  {
    "test 55",
    "test 55: SSE4",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 3,
    /* vaddr= */ 0x80000000,
    "66 0f 38 0a d0 90 90 f4 \n"
  },
  {
    "test 56",
    "test 56: incb decb",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 3,
    /* vaddr= */ 0x80000000,
    "fe 85 4f fd ff ff fe 8d 73 fd ff ff 90 f4 \n"
  },
  {
    "test 57",
    "test 57: lzcnt",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "f3 0f bd 00 90 f4 \n"
  },
  {
    "test 58",
    "test 58: fldz",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "d9 ee 90 f4 \n"
  },
  {
    "test 59",
    "test 59: x87",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 7,
    /* vaddr= */ 0x80000000,
    "dd 9c fd b0 fe ff ff \n"           /* fstpl  -0x150(%ebp,%edi,8)      */
    "dd 9d 40 ff ff ff \n"              /* fstpl  -0xc0(%ebp)              */
    "db 04 24 \n"                       /* fildl  (%esp)                   */
    "dd 5d a0 \n"                       /* fstpl  -0x60(%ebp)              */
    "da e9 \n"                          /* fucompp                         */
    "df e0 \n"                          /* fnstsw %ax                      */
    "90 f4 \n"
  },
  {
    "test 60",
    "test 60: x87 bad instructions",
    /* sawfailure= */ 1, /* illegalinst= */ 9,
    /* instructions= */ 19,
    /* vaddr= */ 0x80000000,
    "dd cc \n"                          /* (bad)                           */
    "dd c0 \n"                          /* ffree  %st(0)                   */
    "dd c7 \n"                          /* ffree  %st(7)                   */
    "dd c8 \n"                          /* (bad)                           */
    "dd cf \n"                          /* (bad)                           */
    "dd f0 \n"                          /* (bad)                           */
    "dd ff \n"                          /* (bad)                           */
    "dd fd \n"                          /* (bad)                           */
    "de d1 \n"                          /* (bad)                           */
    "de d9 \n"                          /* fcompp                          */
    "db 04 24 \n"                       /* fildl  (%esp)                   */
    "dd 5d a0 \n"                       /* fstpl  -0x60(%ebp)              */
    "db e0 \n"                          /* feni(287 only)                  */
    "db ff \n"                          /* (bad)                           */
    "db e8 \n"                          /* fucomi %st(0),%st               */
    "db f7 \n"                          /* fcomi  %st(7),%st               */
    "da e9 \n"                          /* fucompp                         */
    "df e0 \n"                          /* fnstsw %ax                      */
    "90 f4 \n"
  },
  {
    "test 61",
    "test 61: 3DNow prefetch",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "0f 0d 00 \n"                       /* prefetch (%eax)                 */
    "90 f4 \n"
  },
  {
    "test 61.1",
    "test 61.1: F2 0F ...",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 3,
    /* vaddr= */ 0x80000000,
    "f2 0f 48 0f 48 a4 52 \n"
    "f2 0f 10 c8 \n"                    /* movsd  %xmm0,%xmm1              */
    "90 f4 \n"
  },
  {
    "test 62",
    "test 62: f6/f7 test Ib/Iv ...",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 10,
    /* vaddr= */ 0x80000000,
    "f6 c1 ff \n"                       /* test   $0xff,%cl                */
    "f6 44 43 01 02 \n"                 /* testb  $0x2,0x1(%ebx,%eax,2)    */
    "f7 c6 03 00 00 00 \n"              /* test   $0x3,%esi                */
    "90 90 90 90 90 \n"
    "f7 45 18 00 00 00 20 \n"           /* testl  $0x20000000,0x18(%ebp)   */
    "90 f4 \n"
  },
  {
    "test 63",
    "test 63: addr16 corner cases ...",
    /* sawfailure= */ 1, /* illegalinst= */ 4,
    /* instructions= */ 5,
    /* vaddr= */ 0x80000000,
    "67 01 00 \n"                       /* addr16 add %eax,(%bx,%si)       */
    "67 01 40 00 \n"                    /* addr16 add %eax,0x0(%bx,%si)    */
    "67 01 80 00 90 \n"                 /* addr16 add %eax,-0x7000(%bx,%si) */
    "67 01 c0 \n"                       /* addr16 add %eax,%eax            */
    "90 f4 \n"
  },
  {
    "test 64",
    "test 64: text starts with indirect jmp ...",
    /* sawfailure= */ 1, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "ff d0 90 f4 \n"
  },
  {
    "test 65",
    "test 65: nacljmp crosses 32-byte boundary ...",
    /* sawfailure= */ 1, /* illegalinst= */ 0,
    /* instructions= */ 32,
    /* vaddr= */ 0x80000000,
    "90 90 90 90 90 90 90 90 \n"
    "90 90 90 90 90 90 90 90 \n"
    "90 90 90 90 90 90 90 90 \n"
    "90 90 90 90 90 83 e0 ff \n"
    "ff d0 90 f4 \n"
  },
  {
    /* I think this is currently NACLi_ILLEGAL */
    "test 65",
    "test 65: fxsave",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "0f ae 00 00 90 90 90 90 90 f4 \n"
  },
  {
    "test 66",
    "test 66: NACLi_CMPXCHG8B",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "f0 0f c7 08 90 f4 \n"
  },
  {
    "test 67",
    "test 67: NACLi_FCMOV",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 7,
    /* vaddr= */ 0x80000000,
    "da c0 00 00 90 90 90 90 90 f4 \n"
  },
  {
    "test 68",
    "test 68: NACLi_MMX",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 4,
    /* vaddr= */ 0x80000000,
    "0f 60 00 \n" /* punpcklbw (%eax),%mm0 */
    "90 90 90 f4 \n"
  },
  {
    "test 69",
    "test 69: NACLi_SSE",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "0f 5e 90 90 90 90 90 90 f4 \n"
  },
  {
    "test 70",
    "test 70: NACLi_SSE2",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 4,
    /* vaddr= */ 0x80000000,
    "66 0f 60 00 90 90 90 f4 \n"
  },
  {
    "test 71",
    "test 71: NACLi_SSE3",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 4,
    /* vaddr= */ 0x80000000,
    "66 0f 7d 00 90 90 90 f4 \n"
  },
  {
    "test 72",
    "test 72: NACLi_SSE4A",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 4,
    /* vaddr= */ 0x80000000,
    "f2 0f 79 00 90 90 90 f4 \n"
  },
  {
    "test 73",
    "test 73: NACLi_POPCNT",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "f3 0f b8 00 90 f4 \n"
  },
  {
    "test 74",
    "test 74: NACLi_E3DNOW",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "0f 0f 46 01 bb 90 f4 \n"
  },
  {
    "test 75",
    "test 75: NACLi_MMXSSE2",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "66 0f 71 f6 00 90 f4 \n"
  },
  {
    "test 76",
    "test 76: mov eax, ss",
    /* sawfailure= */ 1, /* illegalinst= */ 4,
    /* instructions= */ 4,
    /* vaddr= */ 0x80000000,
    "8e d0 8c d0 66 8c d0 90 f4 \n"
  },
  {
    "test 77",
    "test 77: call esp",
    /* sawfailure= */ 1, /* illegalinst= */ 0,
    /* instructions= */ 3,
    /* vaddr= */ 0x80000000,
    "83 e4 f0 ff d4 90 f4 \n"
  },
  /* code.google.com issue 23 reported by defend.the.world on 11 Dec 2008 */
  {
    "test 78",
    "test 78: call (*edx)",
    /* sawfailure= */ 1, /* illegalinst= */ 0,
    /* instructions= */ 30,
    /* vaddr= */ 0x80000000,
    "90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 \n"
    "90 90 90 90 90 90 90 90 90 90 90 \n"
    "83 e2 e0 \n"                       /* and */
    "ff 12 \n"                          /* call (*edx) */
    "90 f4 \n"                          /* nop halt */
  },
  {
    "test 79",
    "test 79: call *edx",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 30,
    /* vaddr= */ 0x80000000,
    "90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 90 \n"
    "90 90 90 90 90 90 90 90 90 90 90 \n"
    "83 e2 e0 \n"                       /* and */
    "ff d2 \n"                          /* call *edx */
    "90 f4 \n"                          /* nop halt */
  },
  {
    "test 80",
    "test 80: roundss",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 3,
    /* vaddr= */ 0x80000000,
    "66 0f 3a 0a c0 00 \n"              /* roundss $0x0,%xmm0,%xmm0        */
    "90 90 \n"
    "f4 \n"                             /* hlt                             */
  },
  {
    "test 81",
    "test 81: crc32",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 3,
    /* vaddr= */ 0x80000000,
    "f2 0f 38 f1 c8 \n"                 /* crc32l %eax,%ecx                */
    "90 90 \n"
    "f4 \n"                             /* hlt                             */
  },
  {
    "test 82",
    "test 82: SSE4 error 1",
    /* sawfailure= */ 1, /* illegalinst= */ 2,
    /* instructions= */ 4,
    /* vaddr= */ 0x80000000,
    "f3 0f 3a 0e d0 c0 90 f4 \n"
  },
  {
    "test 83",
    "test 83: SSE4 error 2",
    /* sawfailure= */ 1, /* illegalinst= */ 2,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    "f3 0f 38 0f d0 c0 90 f4 \n"
  },
  {
    "test 84",
    "test 84: SSE4 error 3",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 3,
    /* vaddr= */ 0x80000000,
    "66 0f 38 0f d0 c0 90 f4 \n"
  },
  {
    "test 85",
    "test 85: SSE4 error 4",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 3,
    /* vaddr= */ 0x80000000,
    "f2 66 0f 3a 0a c0 00 \n"
    "90 90 \n"
    "f4 \n"                             /* hlt                             */
  },
  {
    "test 86",
    "test 86: bad SSE4 crc32",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 3,
    /* vaddr= */ 0x80000000,
    "f2 f3 0f 38 f1 c8 \n"
    "90 90 \n"
    "f4 \n"                             /* hlt                             */
  },
  {
    "test 87",
    "test 87: bad NACLi_3BYTE instruction (SEGCS prefix)",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 3,
    /* vaddr= */ 0x80000000,
    /* Note: Fixed so that this is a legal instruction,
     * except for the prefix! (karl)
     */
    "2e 0f 3a 0f bb ab 00 00 00 00 \n"
    "90 90 \n"
    "f4 \n"                             /* hlt                             */
  },
  {
    "test 87a",
    "test 87a: bad NACLi_3BYTE instruction (not really an instruction)",
    /* sawfailure= */ 1, /* illegalinst= */ 2,
    /* instructions= */ 2,
    /* vaddr= */ 0x80000000,
    /* Note: Fixed so that this is a legal instruction,
     * except for the prefix! (karl)
     */
    "2e 0f 3a 7d bb ab 00 00 00 00 \n"
    "90 90 \n"
    "f4 \n"                             /* hlt                             */
  },
  {
    "test 88",
    "test 88: two-byte jump with prefix (bug reported by Mark Dowd)",
    /* sawfailure= */ 1, /* illegalinst= */ 1,
    /* instructions= */ 4,
    /* vaddr= */ 0x80000000,
    "66 0f 84 00 00 \n"                 /* data16 je 0x5                   */
    "90 90 \n"
    "f4 \n"                             /* hlt                             */
  },
  {
    "test 89",
    "test 89: sfence",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 8,
    /* vaddr= */ 0x80000000,
    "90 0f ae ff \n"
    "90 90 90 90 90 90 f4 \n"
  },
  {
    "test 90",
    "test 90: clflush",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 8,
    /* vaddr= */ 0x80000000,
    "90 0f ae 3f \n"
    "90 90 90 90 90 90 f4 \n"
  },
  {
    "test 91",
    "test 91: mfence",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 8,
    /* vaddr= */ 0x80000000,
    "90 0f ae f7 \n"
    "90 90 90 90 90 90 f4 \n"
  },
  {
    "test 92",
    "test 92: jump to zero should be allowed",
    /* A jump/call to a zero address will be emitted for a jump/call
       to a weak symbol that is undefined. */
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 1,
    /* vaddr= */ 0x08049000,
    "e9 fb 6f fb f7 \n"                 /* jmp    0                        */
    "f4 \n"                             /* hlt                             */
  },
  {
    "test 93",
    "test 93: jump to bundle-aligned zero page address is currently allowed",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 1,
    /* vaddr= */ 0x08049000,
    "e9 fb 70 fb f7 \n"                 /* jmp    100                      */
    "f4 \n"                             /* hlt                             */
  },
  {
    "test 94",
    "test 94: jump to syscall trampoline should be allowed",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 1,
    /* vaddr= */ 0x08049000,
    "e9 fb 6f fc f7 \n"                 /* jmp    10000                    */
    "f4 \n"                             /* hlt                             */
  },
  {
    "test 95",
    "test 95: unaligned jump to trampoline area must be disallowed",
    /* sawfailure= */ 1, /* illegalinst= */ 0,
    /* instructions= */ 1,
    /* vaddr= */ 0x08049000,
    "e9 fc 6f fc f7 \n"                 /* jmp    10001                    */
    "f4 \n"                             /* hlt                             */
  },
  {
    "test 96",
    "test 96: bundle-aligned jump to before the code chunk is allowed",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 1,
    /* vaddr= */ 0x08049000,
    "e9 fb 6f fb f8 \n"                 /* jmp    1000000                  */
    "f4 \n"                             /* hlt                             */
  },
  {
    "test 97",
    "test 97: bundle-aligned jump to after the code chunk is allowed",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 1,
    /* vaddr= */ 0x08049000,
    "e9 fb 6f fb 07 \n"                 /* jmp    10000000                 */
    "f4 \n"                             /* hlt                             */
  },
};

static void DecodeHexString(const char *input, uint8_t **result_data,
                            size_t *result_size) {
  size_t buf_size = strlen(input) / 2; /* Over-estimate size */
  uint8_t *output;
  uint8_t *buf = malloc(buf_size);
  assert(buf != NULL);

  output = buf;
  while (*input != '\0') {
    if (*input == ' ' || *input == '\n') {
      input++;
    } else {
      char *end;
      assert(output < buf + buf_size);
      *output++ = (uint8_t) strtoul(input, &end, 16);
      /* Expect 2 digits of hex. */
      assert(end == input + 2);
      input = end;
    }
  }
  *result_data = buf;
  *result_size = output - buf;
}

static void TestValidator(struct NCValTestCase *vtest, int didstubout) {
  struct NCValidatorState *vstate;
  uint8_t *byte0;
  size_t data_size;
  int rc;

  DecodeHexString(vtest->data_as_hex, &byte0, &data_size);
  /*
   * The validator used to require that code chunks end in HLT.  We
   * have left the HLTs in, but don't pass them to the validator.
   * TODO(mseaborn): Remove the HLTs.
   */
  assert(byte0[data_size - 1] == 0xf4 /* HLT */);

  vstate = NCValidateInit(vtest->vaddr, data_size - 1,
                          FALSE, &g_ncval_cpu_features);
  assert (vstate != NULL);
  NCValidateSetErrorReporter(vstate, &kNCVerboseErrorReporter);
  NCValidateSegment(byte0, (uint32_t)vtest->vaddr, data_size - 1, vstate);
  free(byte0);
  rc = NCValidateFinish(vstate);

  do {
    printf("vtest->sawfailure = %d, vstate->stats.sawfailure = %d\n",
           vtest->sawfailure, vstate->stats.sawfailure);
    NCStatsPrint(vstate);
    if (vtest->sawfailure != rc) break;
    if (vtest->sawfailure ^ vstate->stats.sawfailure) break;
    if (didstubout != vstate->stats.didstubout) break;
    if (vtest->instructions != vstate->stats.instructions) break;
    if (vtest->illegalinst != vstate->stats.illegalinst) break;
    Info("*** %s passed (%s)\n", vtest->name, vtest->description);
    printf("\n");
    NCValidateFreeState(&vstate);
    return;
  } while (0);
  NCStatsPrint(vstate);
  NCValidateFreeState(&vstate);
  Info("*** %s failed (%s)\n", vtest->name, vtest->description);
  exit(-1);
}

void test_fail_on_bad_alignment(void) {
  struct NCValidatorState *vstate;

   printf("Running test_fail_on_bad_alignment...\n");

  vstate = NCValidateInit(0x80000000, 0x1000, FALSE, &g_ncval_cpu_features);
  CHECK(vstate != NULL);
  NCValidateFreeState(&vstate);

  /* Unaligned start addresses are not allowed. */
  vstate = NCValidateInit(0x80000001, 0x1000, FALSE, &g_ncval_cpu_features);
  CHECK(vstate == NULL);
}

void test_stubout(void) {
  /* Similar to test 68 */
  struct NCValTestCase test = {
    "test stubout",
    "test stubout: NACLi_MMX",
    /* sawfailure= */ 0, /* illegalinst= */ 0,
    /* instructions= */ 1,
    /* vaddr= */ 0x80000000,
    "0f 60 00 f4 \n" /* punpcklbw (%eax),%mm0 */
  };

  printf("Running test_stubout...\n");

  /* If MMX instructions are not allowed, stubout will occur. */
  NaClSetCPUFeatureX86(&g_ncval_cpu_features, NaClCPUFeatureX86_MMX, FALSE);
  TestValidator(&test, TRUE);
  NaClSetCPUFeatureX86(&g_ncval_cpu_features, NaClCPUFeatureX86_MMX, TRUE);
}

void ncvalidate_unittests(void) {
  size_t i;

  /* Default to stubbing out nothing. */
  NaClSetAllCPUFeaturesX86((NaClCPUFeatures *) &g_ncval_cpu_features);

  for (i = 0; i < NACL_ARRAY_SIZE(NCValTests); i++) {
    TestValidator(&NCValTests[i], FALSE);
  }

  test_fail_on_bad_alignment();
  test_stubout();

  Info("\nAll tests passed.\n\n");
}


int main(void) {
  struct GioFile gio_out_stream;
  struct Gio *gout = (struct Gio*) &gio_out_stream;
  if (!GioFileRefCtor(&gio_out_stream, stdout)) {
    fprintf(stderr, "Unable to create gio file for stdout!\n");
    return 1;
  }

  NaClLogModuleInitExtended(LOG_INFO, gout);
  ncvalidate_unittests();
  GioFileDtor(gout);
  return 0;
}
