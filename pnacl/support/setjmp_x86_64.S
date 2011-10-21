/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * jump_buf is organized as follows
 * rbx, rbp, rsp, r12, r13, r14, pc
 * NOTE: r15 is neither saved nor restored
 */
    .text
    .p2align NACLENTRYALIGN
    .globl   setjmp
    .type    setjmp, @function

/* int setjmp(jmp_buf env) */
setjmp:
    /* edi has env argument*/
    pop     %rdx                         /* return address */

    movq    %rbx, %nacl:0(%r15,%rdi)     /* save rbx */
    movq    %rbp, %nacl:8(%r15,%rdi)     /* save rbp */
    movq    %rsp, %nacl:16(%r15,%rdi)    /* save rsp */
    movq    %r12, %nacl:24(%r15,%rdi)    /* save r12 */
    movq    %r13, %nacl:32(%r15,%rdi)    /* save r13 */
    movq    %r14, %nacl:40(%r15,%rdi)    /* save r14 */
    movq    %rdx, %nacl:48(%r15,%rdi)    /* save pc */

    xorq    %rax, %rax
    nacljmp %edx, %r15


    .text
    .p2align NACLENTRYALIGN
    .globl   longjmp
    .type    longjmp, @function

/* void longjmp(jmp_buf env, int val) */
longjmp:
    /* edi has env argument */
    /* esi has val argument */
    movl    %esi, %eax
    /* if val is zero, we must return 1 -- otherwise return val */
    testl   %eax, %eax
    jne     .skip
    movl    $1, %eax
.skip:
    movq    %nacl:0(%r15,%rdi), %rbx     /* restore rbx */
    movq    %nacl:8(%r15,%rdi), %rdx     /* restore rbp */
    naclrestbp %edx, %r15
    movq    %nacl:16(%r15,%rdi), %rdx    /* restore rsp */
    naclrestsp %edx, %r15
    movq    %nacl:24(%r15,%rdi), %r12    /* restore r12 */
    movq    %nacl:32(%r15,%rdi), %r13    /* restore r13 */
    movq    %nacl:40(%r15,%rdi), %r14    /* restore r14 */
    movq    %nacl:48(%r15,%rdi), %rcx
    nacljmp %ecx, %r15
    .p2align NACLENTRYALIGN

