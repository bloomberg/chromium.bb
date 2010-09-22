/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

        /*
         * assume 32-byte bundles
         */
#define GETTIMEOFDAY_ADDR 0x10500

        .data
timebuf:
        .space 0x200 /* enough for a timeval */

        .text
        .globl SyscallReturnIsSandboxed
SyscallReturnIsSandboxed:
        pushl $0
        pushl $timebuf
        pushl $1f

        movl $GETTIMEOFDAY_ADDR, %ecx
        and $0xffffffe0, %ecx
        jmp *%ecx

        .p2align 5
        /* --- this is at the beginning of the bundle --- */
        xorl $1, %eax
1:
        addl $0x8, %esp
        pop %ecx
        and $0xffffffe0, %ecx
        jmp *%ecx
        .p2align 5 /* -- pnacl toolchain leaves "..." without this */
