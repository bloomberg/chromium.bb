/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * ncval_tests.c - simple unit tests for NaCl validator
 */
#include "native_client/src/include/portability.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "native_client/src/trusted/validator_x86/ncvalidate.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_internaltypes.h"

#if !defined(ARRAYSIZE)
#define ARRAYSIZE(a) sizeof(a)/sizeof(a[0])
#endif

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
  int sawfailure;
  uint32_t instructions;
  uint32_t illegalinst;
  int testsize;
  uint32_t vaddr;
  uint8_t *testbytes;
};

struct NCValTestCase NCValTests[] = {
  {
    "test 1",
    "a first very simple test with an illegal inst.",
    1, 9, 1, 26, 0x80000000,
    (uint8_t *)
    "\x55"                              /* push   %ebp                     */
    "\x89\xe5"                          /* mov    %esp,%ebp                */
    "\x83\xec\x08"                      /* sub    $0x8,%esp                */
    "\xe8\x81\x00\x00\x00"              /* call   0x86                     */
    "\xe8\xd3\x00\x00\x00"              /* call   0xd8                     */
    "\xe8\xf3\x04\x00\x00"              /* call   0x4f8                    */
    "\xc9"                              /* leave                           */
    "\xc3"                              /* ret                             */
    "\x00\x00\xf4",
  },
  {
    "test 2",
    "like test 1 but no illegal inst",
    1, 9, 0, 26, 0x80000000,
    (uint8_t *)
    "\x55"                              /* push   %ebp                     */
    "\x89\xe5"                          /* mov    %esp,%ebp                */
    "\x83\xec\x08"                      /* sub    $0x8,%esp                */
    "\xe8\x81\x00\x00\x00"              /* call   0x86                     */
    "\xe8\xd3\x00\x00\x00"              /* call   0xd8                     */
    "\xe8\xf3\x04\x00\x00"              /* call   0x4f8                    */
    "\xc9"                              /* leave                           */
    "\x90"                              /* nop                             */
    "\x00\x00\xf4",
  },
  {
    "test 4",
    "a longer simple test with a bad jump target",
    1, 90, 0, 336, 0x8054600,
    (uint8_t *)
    "\x8d\x4c\x24\x04"                  /* lea    0x4(%esp),%ecx           */
    "\x83\xe4\xf0"                      /* and    $0xfffffff0,%esp         */
    "\xff\x71\xfc"                      /* pushl  -0x4(%ecx)               */
    "\x55"                              /* push   %ebp                     */
    "\x89\xe5"                          /* mov    %esp,%ebp                */
    "\x51"                              /* push   %ecx                     */
    "\x66\x90"                          /* xchg   %ax,%ax                  */
    "\x83\xec\x24"                      /* sub    $0x24,%esp               */
    "\x89\x4d\xe8"                      /* mov    %ecx,-0x18(%ebp)         */
    "\xc7\x45\xf4\x0a\x00\x00\x00"      /* movl   $0xa,-0xc(%ebp)          */
    "\x8b\x45\xe8"                      /* mov    -0x18(%ebp),%eax         */
    "\x83\x38\x01"                      /* cmpl   $0x1,(%eax)              */
    "\x7f\x2b"                          /* jg     0x2d                     */
    "\x8b\x55\xe8"                      /* mov    -0x18(%ebp),%edx         */
    "\x8b\x42\x04"                      /* mov    0x4(%edx),%eax           */
    "\x8b\x00"                          /* mov    (%eax),%eax              */
    "\x8d\x76\x00"                      /* lea    0x0(%esi),%esi           */
    "\x89\x44\x24\x04"                  /* mov    %eax,0x4(%esp)           */
    "\xc7\x04\x24\x54\x14\x00\x08"      /* movl   $0x8001454,(%esp)        */
    "\xe8\xc0\x02\x00\x00"              /* call   0x2c5                    */
    "\xc7\x04\x24\x01\x00\x00\x00"      /* movl   $0x1,(%esp)              */
    "\x8d\x74\x26\x00"                  /* lea    0x0(%esi),%esi           */
    "\xe8\xc0\x01\x00\x00"              /* call   0x1c5                    */
    "\x8b\x55\xe8"                      /* mov    -0x18(%ebp),%edx         */
    "\x8b\x42\x04"                      /* mov    0x4(%edx),%eax           */
    "\x83\xc0\x04"                      /* add    $0x4,%eax                */
    "\x8b\x00"                          /* mov    (%eax),%eax              */
    "\x89\x04\x24"                      /* mov    %eax,(%esp)              */
    "\x66\x90"                          /* xchg   %ax,%ax                  */
    "\x8d\x74\x26\x00"                  /* lea    0x0(%esi),%esi           */
    "\x8d\xbc\x27\x00\x00\x00\x00"      /* lea    0x0(%edi),%edi           */
    "\xe8\x90\x09\x00\x00"              /* call   0x995                    */
    "\x89\x45\xf8"                      /* mov    %eax,-0x8(%ebp)          */
    "\x8b\x45\xe8"                      /* mov    -0x18(%ebp),%eax         */
    "\x83\x38\x02"                      /* cmpl   $0x2,(%eax)              */
    "\x7e\x25"                          /* jle    0x27                     */
    "\x8b\x55\xe8"                      /* mov    -0x18(%ebp),%edx         */
    "\x66\x90"                          /* xchg   %ax,%ax                  */
    "\x8b\x42\x04"                      /* mov    0x4(%edx),%eax           */
    "\x83\xc0\x08"                      /* add    $0x8,%eax                */
    "\x8b\x00"                          /* mov    (%eax),%eax              */
    "\x89\x04\x24"                      /* mov    %eax,(%esp)              */
    "\xe8\x70\x09\x00\x00"              /* call   0x975                    */
    "\x89\x45\xf4"                      /* mov    %eax,-0xc(%ebp)          */
    "\x8d\xb6\x00\x00\x00\x00"          /* lea    0x0(%esi),%esi           */
    "\x8d\xbc\x27\x00\x00\x00\x00"      /* lea    0x0(%edi),%edi           */
    "\x8b\x45\xf4"                      /* mov    -0xc(%ebp),%eax          */
    "\xa3\x28\x2f\x00\x08"              /* mov    %eax,0x8002f28           */
    "\xeb\x26"                          /* jmp    0x28                     */
    "\x8d\xb6\x00\x00\x00\x00"          /* lea    0x0(%esi),%esi           */
    "\xc7\x44\x24\x08\x03\x00\x00\x00"  /* movl   $0x3,0x8(%esp)           */
    "\xc7\x44\x24\x04\x01\x00\x00\x00"  /* movl   $0x1,0x4(%esp)           */
    "\x8b\x45\xf4"                      /* mov    -0xc(%ebp),%eax          */
    "\x89\x04\x24"                      /* mov    %eax,(%esp)              */
    "\x90"                              /* nop                             */
    "\x8d\x74\x26\x00"                  /* lea    0x0(%esi),%esi           */
    "\xe8\x20\x00\x00\x00"              /* call   0x25                     */
    "\x83\x7d\xf8\x00"                  /* cmpl   $0x0,-0x8(%ebp)          */
    "\x0f\x9f\xc0"                      /* setg   %al                      */
    "\x83\x6d\xf8\x01"                  /* subl   $0x1,-0x8(%ebp)          */
    "\x84\xc0"                          /* test   %al,%al                  */
    "\x8d\x76\x00"                      /* lea    0x0(%esi),%esi           */
    "\x75\xce"                          /* jne    0xffffffd0               */
    "\xc7\x04\x24\x00\x00\x00\x00"      /* movl   $0x0,(%esp)              */
    "\x66\x90"                          /* xchg   %ax,%ax                  */
    "\xe8\x20\x01\x00\x00"              /* call   0x125                    */
    "\x55"                              /* push   %ebp                     */
    "\x89\xe5"                          /* mov    %esp,%ebp                */
    "\x83\xec\x1c"                      /* sub    $0x1c,%esp               */
    "\x83\x7d\x08\x01"                  /* cmpl   $0x1,0x8(%ebp)           */
    "\x75\x44"                          /* jne    0x46                     */
    "\x8b\x55\x0c"                      /* mov    0xc(%ebp),%edx           */
    "\x90"                              /* nop                             */
    "\x8b\x04\x95\x24\x2f\x00\x08"      /* mov    0x8002f24(,%edx,4),%eax  */
    "\x83\xe8\x01"                      /* sub    $0x1,%eax                */
    "\x8d\xb6\x00\x00\x00\x00"          /* lea    0x0(%esi),%esi           */
    "\x89\x04\x95\x24\x2f\x00\x08"      /* mov    %eax,0x8002f24(,%edx,4)  */
    "\x8b\x55\x10"                      /* mov    0x10(%ebp),%edx          */
    "\x8d\xb6\x00\x00\x00\x00"          /* lea    0x0(%esi),%esi           */
    "\x8b\x04\x95\x24\x2f\x00\x08"      /* mov    0x8002f24(,%edx,4),%eax  */
    "\x83\xc0\x01"                      /* add    $0x1,%eax                */
    "\x8d\xb6\x00\x00\x00\x00"          /* lea    0x0(%esi),%esi           */
    "\x89\x04\x95\x24\x2f\x00\x08"      /* mov    %eax,0x8002f24(,%edx,4)  */
    "\xeb\x77"                          /* jmp    0x79                     */
    "\x8d\xb4\x26\x00\x00\x00\x00"      /* lea    0x0(%esi),%esi           */
    "\x8b\x45\x10"                      /* mov    0x10(%ebp),%eax          */
    "\x8b\x55\x0c"                      /* mov    0xc(%ebp),%edx           */
    "\x01\xc2"                          /* add    %eax,%edx                */
    "\xb8\x06\x00\x00\x00"              /* mov    $0x6,%eax                */
    "\x29\xd0"                          /* sub    %edx,%eax                */
    "\x90"                              /* nop                             */
  },
  {
    "test 5",
    "like test 4; with bad jump target",
    1, 90, 0, 336, 0x8054600,
    (uint8_t *)
    "\x8d\x4c\x24\x04"                  /* lea    0x4(%esp),%ecx           */
    "\x83\xe4\xf0"                      /* and    $0xfffffff0,%esp         */
    "\xff\x71\xfc"                      /* pushl  -0x4(%ecx)               */
    "\x55"                              /* push   %ebp                     */
    "\x89\xe5"                          /* mov    %esp,%ebp                */
    "\x51"                              /* push   %ecx                     */
    "\x66\x90"                          /* xchg   %ax,%ax                  */
    "\x83\xec\x24"                      /* sub    $0x24,%esp               */
    "\x89\x4d\xe8"                      /* mov    %ecx,-0x18(%ebp)         */
    "\xc7\x45\xf4\x0a\x00\x00\x00"      /* movl   $0xa,-0xc(%ebp)          */
    "\x8b\x45\xe8"                      /* mov    -0x18(%ebp),%eax         */
    "\x83\x38\x01"                      /* cmpl   $0x1,(%eax)              */
    "\x7f\x2b"                          /* jg     0x2d                     */
    "\x8b\x55\xe8"                      /* mov    -0x18(%ebp),%edx         */
    "\x8b\x42\x04"                      /* mov    0x4(%edx),%eax           */
    "\x8b\x00"                          /* mov    (%eax),%eax              */
    "\x8d\x76\x00"                      /* lea    0x0(%esi),%esi           */
    "\x89\x44\x24\x04"                  /* mov    %eax,0x4(%esp)           */
    "\xc7\x04\x24\x54\x14\x00\x08"      /* movl   $0x8001454,(%esp)        */
    "\xe8\xc0\x02\x00\x00"              /* call   0x2c5                    */
    "\xc7\x04\x24\x01\x00\x00\x00"      /* movl   $0x1,(%esp)              */
    "\x8d\x74\x26\x00"                  /* lea    0x0(%esi),%esi           */
    "\xe8\xc0\x01\x00\x00"              /* call   0x1c5                    */
    "\x8b\x55\xe8"                      /* mov    -0x18(%ebp),%edx         */
    "\x8b\x42\x04"                      /* mov    0x4(%edx),%eax           */
    "\x83\xc0\x04"                      /* add    $0x4,%eax                */
    "\x8b\x00"                          /* mov    (%eax),%eax              */
    "\x89\x04\x24"                      /* mov    %eax,(%esp)              */
    "\x66\x90"                          /* xchg   %ax,%ax                  */
    "\x8d\x74\x26\x00"                  /* lea    0x0(%esi),%esi           */
    "\x8d\xbc\x27\x00\x00\x00\x00"      /* lea    0x0(%edi),%edi           */
    "\xe8\x90\x09\x00\x00"              /* call   0x995                    */
    "\x89\x45\xf8"                      /* mov    %eax,-0x8(%ebp)          */
    "\x8b\x45\xe8"                      /* mov    -0x18(%ebp),%eax         */
    "\x83\x38\x02"                      /* cmpl   $0x2,(%eax)              */
    "\x7e\x25"                          /* jle    0x27                     */
    "\x8b\x55\xe8"                      /* mov    -0x18(%ebp),%edx         */
    "\x66\x90"                          /* xchg   %ax,%ax                  */
    "\x8b\x42\x04"                      /* mov    0x4(%edx),%eax           */
    "\x83\xc0\x08"                      /* add    $0x8,%eax                */
    "\x8b\x00"                          /* mov    (%eax),%eax              */
    "\x89\x04\x24"                      /* mov    %eax,(%esp)              */
    "\xe8\x70\x09\x00\x00"              /* call   0x975                    */
    "\x89\x45\xf4"                      /* mov    %eax,-0xc(%ebp)          */
    "\x8d\xb6\x00\x00\x00\x00"          /* lea    0x0(%esi),%esi           */
    "\x8d\xbc\x27\x00\x00\x00\x00"      /* lea    0x0(%edi),%edi           */
    "\x8b\x45\xf4"                      /* mov    -0xc(%ebp),%eax          */
    "\xa3\x28\x2f\x00\x08"              /* mov    %eax,0x8002f28           */
    "\xeb\x26"                          /* jmp    0x28                     */
    "\x8d\xb6\x00\x00\x00\x00"          /* lea    0x0(%esi),%esi           */
    "\xc7\x44\x24\x08\x03\x00\x00\x00"  /* movl   $0x3,0x8(%esp)           */
    "\xc7\x44\x24\x04\x01\x00\x00\x00"  /* movl   $0x1,0x4(%esp)           */
    "\x8b\x45\xf4"                      /* mov    -0xc(%ebp),%eax          */
    "\x89\x04\x24"                      /* mov    %eax,(%esp)              */
    "\x90"                              /* nop                             */
    "\x8d\x74\x26\x00"                  /* lea    0x0(%esi),%esi           */
    "\xe8\x20\x00\x00\x00"              /* call   0x25                     */
    "\x83\x7d\xf8\x00"                  /* cmpl   $0x0,-0x8(%ebp)          */
    "\x0f\x9f\xc0"                      /* setg   %al                      */
    "\x83\x6d\xf8\x01"                  /* subl   $0x1,-0x8(%ebp)          */
    "\x84\xc0"                          /* test   %al,%al                  */
    "\x8d\x76\x00"                      /* lea    0x0(%esi),%esi           */
    "\x75\xce"                          /* jne    0xffffffd0               */
    "\xc7\x04\x24\x00\x00\x00\x00"      /* movl   $0x0,(%esp)              */
    "\x66\x90"                          /* xchg   %ax,%ax                  */
    "\xe8\x20\x01\x00\x00"              /* call   0x125                    */
    "\x55"                              /* push   %ebp                     */
    "\x89\xe5"                          /* mov    %esp,%ebp                */
    "\x83\xec\x1c"                      /* sub    $0x1c,%esp               */
    "\x83\x7d\x08\x01"                  /* cmpl   $0x1,0x8(%ebp)           */
    "\x75\x44"                          /* jne    0x46                     */
    "\x8b\x55\x0c"                      /* mov    0xc(%ebp),%edx           */
    "\x90"                              /* nop                             */
    "\x8b\x04\x95\x24\x2f\x00\x08"      /* mov    0x8002f24(,%edx,4),%eax  */
    "\x83\xe8\x01"                      /* sub    $0x1,%eax                */
    "\x8d\xb6\x00\x00\x00\x00"          /* lea    0x0(%esi),%esi           */
    "\x89\x04\x95\x24\x2f\x00\x08"      /* mov    %eax,0x8002f24(,%edx,4)  */
    "\x8b\x55\x10"                      /* mov    0x10(%ebp),%edx          */
    "\x8d\xb6\x00\x00\x00\x00"          /* lea    0x0(%esi),%esi           */
    "\x8b\x04\x95\x24\x2f\x00\x08"      /* mov    0x8002f24(,%edx,4),%eax  */
    "\x83\xc0\x01"                      /* add    $0x1,%eax                */
    "\x8d\xb6\x00\x00\x00\x00"          /* lea    0x0(%esi),%esi           */
    "\x89\x04\x95\x24\x2f\x00\x08"      /* mov    %eax,0x8002f24(,%edx,4)  */
    "\x00\x00"                          /* add    %al,(%eax)               */
    "\x8d\xb4\x26\x00\x00\x00\x00"      /* lea    0x0(%esi),%esi           */
    "\x8b\x45\x10"                      /* mov    0x10(%ebp),%eax          */
    "\x8b\x55\x0c"                      /* mov    0xc(%ebp),%edx           */
    "\x01\xc2"                          /* add    %eax,%edx                */
    "\xb8\x06\x00\x00\x00"              /* mov    $0x6,%eax                */
    "\x29\xd0"                          /* sub    %edx,%eax                */
    "\xf4"                              /* hlt                             */
  },
  {
    "test 6",
    "test 6: 3c 25   cmp %al, $I",
    0, 7, 0, 9, 0x80000000,
    (uint8_t *)
    "\x3c\x25"                          /* cmp    $0x25,%al                */
    "\x90\x90\x90\x90\x90\x90\xf4"
  },
  {
    "test 7",
    "test 7: group2, three byte move",
    0, 8, 0, 13, 0x80000000,
    (uint8_t *)"\xc1\xf9\x1f\x89\x4d\xe4"
    "\x90\x90\x90\x90\x90\x90\xf4"
  },
  {
    "test 8",
    "test 8: five byte move",
    0, 7, 0, 12, 0x80000000,
    (uint8_t *)
    "\xc6\x44\x05\xd6\x00"              /* movb   $0x0,-0x2a(%ebp,%eax,1)  */
    "\x90\x90\x90\x90\x90\x90\xf4"
  },
  {
    "test 9",
    "test 9: seven byte control transfer, unprotected",
    1, 7, 0, 14, 0x80000000,
    (uint8_t *)
    "\xff\x24\x95\xc8\x6e\x05\x08"      /* jmp    *0x8056ec8(,%edx,4)      */
    "\x90\x90\x90\x90\x90\x90\xf4"
  },
  {
    "test 10",
    "test 10: eight byte bts instruction",
    1, 7, 1, 15, 0x80000000,
    (uint8_t *)
    "\x0f\xab\x14\x85\x40\xfb\x27\x08"  /* bts    %edx,0x827fb40(,%eax,4)  */
    "\x90\x90\x90\x90\x90\x90\xf4",
  },
  {
    "test 11",
    "test 11: four byte move",
    0, 7, 0, 11, 0x80000000,
    (uint8_t *)
    "\x66\xbf\x08\x00"                  /* mov    $0x8,%di                 */
    "\x90\x90\x90\x90\x90\x90\xf4",
  },
  {
    "test 12",
    "test 12: five byte movsx",
    0, 7, 0, 12, 0x80000000,
    (uint8_t *)
    "\x66\x0f\xbe\x04\x10"              /* movsbw (%eax,%edx,1),%ax        */
    "\x90\x90\x90\x90\x90\x90\xf4"
  },
  {
    "test 13",
    "test 13: eight byte bts instruction, missing full stop",
    1, 7, 1, 15, 0x80000000,
    (uint8_t *)
    "\x0f\xab\x14\x85\x40\xfb\x27\x08"  /* bts    %edx,0x827fb40(,%eax,4)  */
    "\x90\x90\x90\x90\x90\x90\x90",
  },
  /* ldmxcsr, stmxcsr */
  {
    "test 14",
    "test 14: ldmxcsr, stmxcsr",
    1, 10, 2, 15, 0x80000000,
    (uint8_t *)"\x90\x0f\xae\x10\x90\x0f\xae\x18"
    "\x90\x90\x90\x90\x90\x90\xf4",
  },
  /* invalid */
  {
    "test 15",
    "test 15: invalid instruction",
    1, 8, 1, 11, 0x80000000,
    (uint8_t *)"\x90\x0f\xae\x21"
    "\x90\x90\x90\x90\x90\x90\xf4",
  },
  /* lfence */
  {
    "test 16",
    "test 16: lfence",
    0, 8, 0, 11, 0x80000000,
    (uint8_t *)"\x90\x0f\xae\xef"
    "\x90\x90\x90\x90\x90\x90\xf4",
  },
  {
    "test 17",
    "test 17: lock cmpxchg",
    0, 4, 0, 12, 0x80000000,
    (uint8_t *)
    "\xf0\x0f\xb1\x8f\xa8\x01\x00\x00"  /* lock cmpxchg %ecx,0x1a8(%edi)   */
    "\x90\x90\x90\xf4",
  },
  {
    "test 18",
    "test 18: loop branch into overlapping instruction",
    1, 3, 1, 10, 0x80000000,
    (uint8_t *)"\xbb\x90\x40\xcd\x80\x85\xc0\xe1\xf8\xf4",
  },
  {
    "test 19",
    "test 19: aad test",
    1, 5, 2, 15, 0x80000000,
    (uint8_t *)"\x68\x8a\x80\x04\x08\xd5\xb0\xc3\x90\xbb\x90\x40\xcd\x80\xf4"
  },
  {
    "test 20",
    "test 20: addr16 lea",
    1, 5, 2, 19, 0x80000000,
    (uint8_t *)"\x68\x8e\x80\x04\x08\x66\x67\x8d\x98\xff\xff\xc3\x90\xbb\x90\x40\xcd\x80\xf4"
  },
  {
    "test 21",
    "test 21: aam",
    1, 4, 2, 14, 0x80000000,
    (uint8_t *)"\x68\x89\x80\x04\x08\xd4\xb0\xc3\xbb\x90\x40\xcd\xf4",
  },
  {
    "test 22",
    "test 22: pshufw",
    1, 4, 1, 16, 0x80000000,
    (uint8_t *)"\x68\x8b\x80\x04\x08\x0f\x70\xca\xb3\xc3\xbb\x90\x40\xcd\x80\xf4",
  },
  {
    "test 23",
    "test 23: 14-byte nacljmp using eax",
    1, 3, 0, 15, 0x80000000,
    (uint8_t *)"\x81\xe0\xff\xff\xff\xff\x81\xc8\x00\x00\x00\x00\xff\xd0\xf4",
  },
  {
    "test 24",
    "test 24: 5-byte nacljmp",
    0, 2, 0, 6, 0x80000000,
    (uint8_t *)"\x83\xe0\xf0\xff\xe0\xf4",
  },
  {
    "test 25",
    "test 25: 0xe3 jmp",
    1, 1, 1, 3, 0x80000000,
    (uint8_t *)"\xe3\x00\xf4",
  },
  {
    "test 26",
    "test 26: 0xe9 jmp, nop",
    0, 2, 0, 7, 0x80000000,
    (uint8_t *)"\xe9\x00\x00\x00\x00\x90\xf4",
  },
  {
    "test 27",
    "test 27: 0xf0 0x80 jmp, nop",
    0, 2, 0, 8, 0x80000000,
    (uint8_t *)"\x0f\x80\x00\x00\x00\x00\x90\xf4",
  },
  {
    "test 28",
    "test 28: 0xe9 jmp",
    1, 1, 0, 6, 0x80000000,
    (uint8_t *)"\xe9\x00\x00\x00\x00\xf4",
  },
  {
    "test 30",
    "test 30: addr16 lea ret",
    1, 3, 2, 8, 0x80000000,
    (uint8_t *)"\x67\x8d\xb4\x9a\x40\xc3\x90\xf4",
  },
  {
    "test 31",
    "test 31: repz movsbl",
    1, 3, 2, 8, 0x80000000,
    (uint8_t *)"\xf3\x0f\xbe\x40\xd0\xc3\x90\xf4",
  },
  {
    "test 32",
    "test 32: infinite loop",
    0, 1, 0, 3, 0x80000000,
    (uint8_t *)"\x7f\xfe\xf4",
  },
  {
    "test 33",
    "test 33: bad branch",
    1, 1, 0, 3, 0x80000000,
    (uint8_t *)"\x7f\xfd\xf4",
  },
  {
    "test 34",
    "test 34: bad branch",
    1, 1, 0, 3, 0x80000000,
    (uint8_t *)"\x7f\xff\xf4",
  },
  {
    "test 35",
    "test 35: bad branch",
    1, 1, 0, 3, 0x80000000,
    (uint8_t *)"\x7f\x00\xf4",
  },
  {
    "test 36",
    "test 36: bad branch",
    1, 1, 0, 3, 0x80000000,
    (uint8_t *)"\x7f\x01\xf4",
  },
  {
    "test 37",
    "test 37: bad branch",
    1, 1, 0, 3, 0x80000000,
    (uint8_t *)"\x7f\x02\xf4",
  },
  {
    "test 38",
    "test 38: intc",
    1, 10, 8, 18, 0x80000000,
    (uint8_t *)"\x66\xeb\x1b\x31\x51\x3d\xef\xcc\x2f\x36\x48\x6e\x44\x2e\xcc\x14\xf4\xf4",
  },
  {
    "test 39",
    "test 39: bad branch",
    1, 7, 2, 18, 0x80000000,
    (uint8_t *)"\x67\x8d\x1d\x22\xa0\x05\xe3\x7b\x9c\xdb\x08\x04\xb1\x90\xed\x12\xf4\xf4",
  },
  {
    "test 40",
    "test 40: more addr16 problems",
    1, 4, 2, 9, 0x80000000,
    (uint8_t *)"\x67\xa0\x00\x00\xcd\x80\x90\x90\xf4",
  },
  {
    "test 41",
    "test 41: the latest non-bug from hcf",
    1, 5, 1, 17, 0x80000000,
    (uint8_t *)"\x84\xd4\x04\x53\xa0\x04\x6a\x5a\x20\xcc\xb8\x48\x03\x2b\x96\x11\xf4"
  },
  {
    "test 42",
    "test 42: another case from hcf",
    1, 7, 1, 17, 0x80000000,
    (uint8_t *)"\x45\x7f\x89\x58\x94\x04\x24\x1b\xc3\xe2\x6f\x1a\x94\x87\x8f\x0b\xf4",
  },
  {
    "test 43",
    "test 43: too many prefix bytes",
    1, 2, 1, 8, 0x80000000,
    (uint8_t *)"\x66\x66\x66\x66\x00\x00\x90\xf4"
  },
  {
    "test 44",
    "test 44: palignr (SSSE3)",
    0, 2, 0, 8, 0x80000000,
    (uint8_t *)"\x66\x0f\x3a\x0f\xd0\xc0\x90\xf4"
  },
  {
    "test 45",
    "test 45: undefined inst in 3-byte opcode space",
    1, 2, 2, 8, 0x80000000,
    (uint8_t *)"\x66\x0f\x39\x0f\xd0\xc0\x90\xf4"
  },
  {
    "test 46",
    "test 46: SSE2x near miss",
    1, 2, 1, 7, 0x80000000,
    (uint8_t *)"\x66\x0f\x73\x00\x00\x90\xf4"
  },
  {
    "test 47",
    "test 47: SSE2x",
    0, 2, 0, 7, 0x80000000,
    (uint8_t *)"\x66\x0f\x73\xff\x00\x90\xf4"
  },
  {
    "test 48",
    "test 48: SSE2x, missing required prefix byte",
    1, 2, 1, 6, 0x80000000,
    (uint8_t *)"\x0f\x73\xff\x00\x90\xf4"
  },
  {
    "test 49",
    "test 49: 3DNow example",
    0, 2, 0, 7, 0x80000000,
    (uint8_t *)"\x0f\x0f\x46\x01\xbf\x90\xf4"
  },
  {
    "test 50",
    "test 50: 3DNow error example 1",
    1, 2, 1, 7, 0x80000000,
    (uint8_t *)"\x0f\x0f\x46\x01\x00\x90\xf4"
  },
  {
    "test 51",
    "test 51: 3DNow error example 2",
    1, 0, 0, 5, 0x80000000,
    (uint8_t *)"\x0f\x0f\x46\x01\xf4"
  },
  {
    "test 52",
    "test 52: 3DNow error example 3",
    1, 2, 1, 7, 0x80000000,
    (uint8_t *)"\x0f\x0f\x46\x01\xbe\x90\xf4"
  },
  {
    "test 53",
    "test 53: 3DNow error example 4",
    1, 2, 1, 7, 0x80000000,
    (uint8_t *)"\x0f\x0f\x46\x01\xaf\x90\xf4"
  },
  {
    "test 54",
    "test 54: SSE4",
    0, 2, 0, 8, 0x80000000,
    (uint8_t *)"\x66\x0f\x3a\x0e\xd0\xc0\x90\xf4"
  },
  {
    "test 55",
    "test 55: SSE4",
    0, 3, 0, 8, 0x80000000,
    (uint8_t *)"\x66\x0f\x38\x0a\xd0\x90\x90\xf4"
  },
  {
    "test 56",
    "test 56: incb decb",
    0, 3, 0, 14, 0x80000000,
    (uint8_t *)"\xfe\x85\x4f\xfd\xff\xff\xfe\x8d\x73\xfd\xff\xff\x90\xf4",
  },
  {
    "test 57",
    "test 57: lzcnt",
    0, 2, 0, 6, 0x80000000,
    (uint8_t *)"\xf3\x0f\xbd\x00\x90\xf4",
  },
  {
    "test 58",
    "test 58: fldz",
    0, 2, 0, 4, 0x80000000,
    (uint8_t *)"\xd9\xee\x90\xf4",
  },
  {
    "test 59",
    "test 59: x87",
    0, 7, 0, 25, 0x80000000,
    (uint8_t *)
    "\xdd\x9c\xfd\xb0\xfe\xff\xff"      /* fstpl  -0x150(%ebp,%edi,8)      */
    "\xdd\x9d\x40\xff\xff\xff"          /* fstpl  -0xc0(%ebp)              */
    "\xdb\x04\x24"                      /* fildl  (%esp)                   */
    "\xdd\x5d\xa0"                      /* fstpl  -0x60(%ebp)              */
    "\xda\xe9"                          /* fucompp                         */
    "\xdf\xe0"                          /* fnstsw %ax                      */
    "\x90\xf4",
  },
  {
    "test 60",
    "test 60: x87 bad instructions",
    1, 19, 9, 40, 0x80000000,
    (uint8_t *)
    "\xdd\xcc"                          /* (bad)                           */
    "\xdd\xc0"                          /* ffree  %st(0)                   */
    "\xdd\xc7"                          /* ffree  %st(7)                   */
    "\xdd\xc8"                          /* (bad)                           */
    "\xdd\xcf"                          /* (bad)                           */
    "\xdd\xf0"                          /* (bad)                           */
    "\xdd\xff"                          /* (bad)                           */
    "\xdd\xfd"                          /* (bad)                           */
    "\xde\xd1"                          /* (bad)                           */
    "\xde\xd9"                          /* fcompp                          */
    "\xdb\x04\x24"                      /* fildl  (%esp)                   */
    "\xdd\x5d\xa0"                      /* fstpl  -0x60(%ebp)              */
    "\xdb\xe0"                          /* feni(287 only)                  */
    "\xdb\xff"                          /* (bad)                           */
    "\xdb\xe8"                          /* fucomi %st(0),%st               */
    "\xdb\xf7"                          /* fcomi  %st(7),%st               */
    "\xda\xe9"                          /* fucompp                         */
    "\xdf\xe0"                          /* fnstsw %ax                      */
    "\x90\xf4",
  },
  {
    "test 61",
    "test 61: 3DNow prefetch",
    0, 2, 0, 5, 0x80000000,
    (uint8_t *)
    "\x0f\x0d\x00"                      /* prefetch (%eax)                 */
    "\x90\xf4",
  },
  {
    "test 61.1",
    "test 61.1: F2 0F ...",
    1, 3, 1, 13, 0x80000000,
    (uint8_t *)"\xf2\x0f\x48\x0f\x48\xa4\x52"
    "\xf2\x0f\x10\xc8"                  /* movsd  %xmm0,%xmm1              */
    "\x90\xf4",
  },
  {
    "test 62",
    "test 62: f6/f7 test Ib/Iv ...",
    0, 10, 0, 28, 0x80000000,
    (uint8_t *)
    "\xf6\xc1\xff"                      /* test   $0xff,%cl                */
    "\xf6\x44\x43\x01\x02"              /* testb  $0x2,0x1(%ebx,%eax,2)    */
    "\xf7\xc6\x03\x00\x00\x00"          /* test   $0x3,%esi                */
    "\x90\x90\x90\x90\x90"
    "\xf7\x45\x18\x00\x00\x00\x20"      /* testl  $0x20000000,0x18(%ebp)   */
    "\x90\xf4",
  },
  {
    "test 63",
    "test 63: addr16 corner cases ...",
    1, 5, 4, 17, 0x80000000,
    (uint8_t *)
    "\x67\x01\x00"                      /* addr16 add %eax,(%bx,%si)       */
    "\x67\x01\x40\x00"                  /* addr16 add %eax,0x0(%bx,%si)    */
    "\x67\x01\x80\x00\x90"              /* addr16 add %eax,-0x7000(%bx,%si) */
    "\x67\x01\xc0"                      /* addr16 add %eax,%eax            */
    "\x90\xf4",
  },
  {
    "test 64",
    "test 64: text starts with indirect jmp ...",
    1, 2, 0, 4, 0x80000000,
    (uint8_t *)"\xff\xd0\x90\xf4"
  },
  {
    "test 65",
    "test 65: nacljmp crosses 32-byte boundary ...",
    1, 32, 0, 36, 0x80000000,
    (uint8_t *)"\x90\x90\x90\x90\x90\x90\x90\x90"
    "\x90\x90\x90\x90\x90\x90\x90\x90"
    "\x90\x90\x90\x90\x90\x90\x90\x90"
    "\x90\x90\x90\x90\x90\x83\xe0\xff"
    "\xff\xd0\x90\xf4"
  },
  {
    /* I think this is currently NACLi_ILLEGAL */
    "test 65",
    "test 65: fxsave",
    1, 2, 1, 10, 0x80000000,
    (uint8_t *)"\x0f\xae\x00\x00\x90\x90\x90\x90\x90\xf4"
  },
  {
    "test 66",
    "test 66: NACLi_CMPXCHG8B",
    0, 2, 0, 6, 0x80000000,
    (uint8_t *)"\xf0\x0f\xc7\010\x90\xf4"
  },
  {
    "test 67",
    "test 67: NACLi_FCMOV",
    0, 7, 0, 10, 0x80000000,
    (uint8_t *)"\xda\xc0\x00\x00\x90\x90\x90\x90\x90\xf4"
  },
  {
    "test 68",
    "test 68: NACLi_MMX",
    0, 4, 0, 7, 0x80000000,
    (uint8_t *)"\x0f\x60\x00\x90\x90\x90\xf4"
  },
  {
    "test 69",
    "test 69: NACLi_SSE",
    0, 2, 0, 9, 0x80000000,
    (uint8_t *)"\x0f\x5e\x90\x90\x90\x90\x90\x90\xf4"
  },
  {
    "test 70",
    "test 70: NACLi_SSE2",
    0, 4, 0, 8, 0x80000000,
    (uint8_t *)"\x66\x0f\x60\x00\x90\x90\x90\xf4"
  },
  {
    "test 71",
    "test 71: NACLi_SSE3",
    0, 4, 0, 8, 0x80000000,
    (uint8_t *)"\x66\x0f\x7d\x00\x90\x90\x90\xf4"
  },
  {
    "test 72",
    "test 72: NACLi_SSE4A",
    0, 4, 0, 8, 0x80000000,
    (uint8_t *)"\xf2\x0f\x79\x00\x90\x90\x90\xf4"
  },
  {
    "test 73",
    "test 73: NACLi_POPCNT",
    0, 2, 0, 6, 0x80000000,
    (uint8_t *)"\xf3\x0f\xb8\x00\x90\xf4"
  },
  {
    "test 74",
    "test 74: NACLi_E3DNOW",
    0, 2, 0, 7, 0x80000000,
    (uint8_t *)"\x0f\x0f\x46\x01\xbb\x90\xf4"
  },
  {
    "test 75",
    "test 75: NACLi_MMXSSE2",
    0, 2, 0, 7, 0x80000000,
    (uint8_t *)"\x66\x0f\x71\xf6\x00\x90\xf4",
  },
  {
    "test 76",
    "test 76: mov eax, ss",
    1, 4, 4, 9, 0x80000000,
    (uint8_t *)"\x8e\xd0\x8c\xd0\x66\x8c\xd0\x90\xf4",
  },
  {
    "test 77",
    "test 77: call esp",
    1, 3, 0, 7, 0x80000000,
    (uint8_t *)"\x83\xe4\xf0\xff\xd4\x90\xf4",
  },
  /* code.google.com issue 23 reported by defend.the.world on 11 Dec 2008 */
  {
    "test 78",
    "test 78: call (*edx)",
    1, 30, 0, 34, 0x80000000,
    (uint8_t *)
    "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90"
    "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90"
    "\x83\xe2\xf0" /* and */
    "\xff\x12"     /* call (*edx) */
    "\x90\xf4",    /* nop halt */
  },
  {
    "test 79",
    "test 79: call *edx",
    0, 30, 0, 34, 0x80000000,
    (uint8_t *)
    "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90"
    "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90"
    "\x83\xe2\xf0" /* and */
    "\xff\xd2"     /* call *edx */
    "\x90\xf4",    /* nop halt */
  },
  {
    "test 80",
    "test 80: roundss",
    0, 3, 0, 9, 0x80000000,
    (uint8_t *)
    "\x66\x0f\x3a\x0a\xc0\x00"          /* roundss $0x0,%xmm0,%xmm0        */
    "\x90\x90"
    "\xf4"                              /* hlt                             */
  },
  {
    "test 81",
    "test 81: crc32",
    0, 3, 0, 8, 0x80000000,
    (uint8_t *)
    "\xf2\x0f\x38\xf1\xc8"              /* crc32l %eax,%ecx                */
    "\x90\x90"
    "\xf4"                              /* hlt                             */
  },
  {
    "test 82",
    "test 82: SSE4 error 1",
    1, 4, 2, 8, 0x80000000,
    (uint8_t *)"\xf3\x0f\x3a\x0e\xd0\xc0\x90\xf4"
  },
  {
    "test 83",
    "test 83: SSE4 error 2",
    1, 2, 2, 8, 0x80000000,
    (uint8_t *)"\xf3\x0f\x38\x0f\xd0\xc0\x90\xf4"
  },
  {
    "test 84",
    "test 84: SSE4 error 3",
    1, 3, 1, 8, 0x80000000,
    (uint8_t *)"\x66\x0f\x38\x0f\xd0\xc0\x90\xf4"
  },
  {
    "test 85",
    "test 85: SSE4 error 4",
    1, 3, 1, 10, 0x80000000,
    (uint8_t *)"\xf2\x66\x0f\x3a\x0a\xc0\x00"
    "\x90\x90"
    "\xf4"                              /* hlt                             */
  },
  {
    "test 86",
    "test 86: bad SSE4 crc32",
    1, 3, 1, 9, 0x80000000,
    (uint8_t *)"\xf2\xf3\x0f\x38\xf1\xc8"
    "\x90\x90"
    "\xf4"                              /* hlt                             */
  },
  {
    "test 87",
    "test 87: bad NACLi_3BYTE instruction (SEGCS prefix)",
    1, 3, 1, 13, 0x80000000,
    (uint8_t *)"\x2e\x0f\x3a\x7d\xbb\xab\x00\x00\x00\x00"
    "\x90\x90"
    "\xf4"                              /* hlt                             */
  },
  {
    "test 88",
    "test 88: two-byte jump with prefix (bug reported by Mark Dowd)",
    1, 4, 1, 8, 0x80000000,
    (uint8_t *)
    "\x66\x0f\x84\x00\x00"              /* data16 je 0x5                   */
    "\x90\x90"
    "\xf4"                              /* hlt                             */
  },
  {
    "test 89",
    "test 89: sfence",
    0, 8, 0, 11, 0x80000000,
    (uint8_t *)"\x90\x0f\xae\xff"
    "\x90\x90\x90\x90\x90\x90\xf4",
  },
  {
    "test 90",
    "test 90: clflush",
    0, 8, 0, 11, 0x80000000,
    (uint8_t *)"\x90\x0f\xae\x3f"
    "\x90\x90\x90\x90\x90\x90\xf4",
  },
  {
    "test 91",
    "test 91: mfence",
    0, 8, 0, 11, 0x80000000,
    (uint8_t *)"\x90\x0f\xae\xf7"
    "\x90\x90\x90\x90\x90\x90\xf4",
  },

};

static uint8_t *memdup(uint8_t *s, int len) {
  return memcpy(malloc(len), s, len);
}

static void TestValidator(struct NCValTestCase *vtest) {
  struct NCValidatorState *vstate;
  uint8_t *byte0 = memdup(vtest->testbytes, vtest->testsize);
  int rc;

  vstate = NCValidateInit(vtest->vaddr,
                          vtest->vaddr + vtest->testsize, 16);
  assert (vstate != NULL);
  NCValidateSegment(byte0, (uint32_t)vtest->vaddr, vtest->testsize, vstate);
  free(byte0);
  rc = NCValidateFinish(vstate);
  do {
    if (vtest->sawfailure ^ vstate->stats.sawfailure) break;
    if (vtest->instructions != vstate->stats.instructions) break;
    if (vtest->illegalinst != vstate->stats.illegalinst) break;
    Info("*** %s passed (%s)\n", vtest->name, vtest->description);
    NCValidateFreeState(&vstate);
    return;
  } while (0);
  Stats_Print(stderr, vstate);
  NCValidateFreeState(&vstate);
  Info("*** %s failed (%s)\n", vtest->name, vtest->description);
  exit(-1);
}

void test_fail_on_bad_alignment() {
  struct NCValidatorState *vstate;

  vstate = NCValidateInit(0x80000000, 0x80001000, 16);
  assert (vstate != NULL);
  NCValidateFreeState(&vstate);

  /* Unaligned start addresses are not allowed. */
  vstate = NCValidateInit(0x80000001, 0x80001000, 16);
  assert (vstate == NULL);

  /* Only alignments of 32 and 64 bytes are supported. */
  vstate = NCValidateInit(0x80000000, 0x80001000, 64);
  assert (vstate == NULL);
}

void ncvalidate_unittests() {
  size_t i;

  for (i = 0; i < ARRAYSIZE(NCValTests); i++) {
    TestValidator(&NCValTests[i]);
  }

  test_fail_on_bad_alignment();

  Info("\nAll tests passed.\n\n");
}


int main() {
  ncvalidate_unittests();
  return 0;
}
