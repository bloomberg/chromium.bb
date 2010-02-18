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
        movq $0, %rsi
        movq $timebuf, %rdi
        movl $1f, %ecx
        addq %r15, %rcx
        pushq %rcx

        movq $ GETTIMEOFDAY_ADDR, %rcx
        andl $0xffffffe0, %ecx
        addq %r15, %rcx
        jmp *%rcx

        .p2align 5
        /* --- this is at the beginning of the bundle --- */
        xorq $1, %rax
1:
        pop %rcx
        andl $0xffffffe0, %ecx
        addq %r15, %rcx
        jmp *%rcx
