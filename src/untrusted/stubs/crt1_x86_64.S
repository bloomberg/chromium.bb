/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* Native Client crt0 startup code for x86-64*/

    .text
    .p2align NACLENTRYALIGN,0xf4

    .global _start
_start:
    /*
     *  The i386 ELF ABI specifies that on entry the stack looks like:
     *            --------------------------------
     *            |          Unspecified         |
     *            --------------------------------
     *            | Information block, including |
     *            |       argument strings,      |
     *            |      environment strings,    |
     *            |     auxiliary information    |
     *            |              ...             |
     *            |        (size varies)         |
     *            --------------------------------
     *            |         Unspecified          |
     *            --------------------------------
     *            | Null auxiliary vector entry  |
     *            --------------------------------
     *            |        Auxiliary vector      |
     *            |              ...             |
     *            |        (2-word entries)      |
     *            --------------------------------
     *            |            0 word            |
     *            --------------------------------
     *            |      Environment pointers    |
     *            |              ...             |
     *            |         (one word each)      |
     *            --------------------------------
     *            |       Argument pointers      |
     *            |              ...             |
     *  4(%esp)   |    (Argument count words)    |
     *            --------------------------------
     *  0(%esp)   |        Argument count        |
     *            --------------------------------
     *            |           Undefined          |
     *            --------------------------------
     *  TODO(sehr): fix stack alignments of atexit, _init, _fini, and
     *              exit.
     */

    /*
     *  Because we are going to align the stack 0mod16 for SSE2,
     *  We need to gather the argc, argv, and envp pointers before
     *  moving esp.
     */
    popq    %rsi               /* Remove argc from the top of the stack */
    movq    %rsp, %rcx         /* Save the argv pointer */

    /*
     * Finding envp requires skipping over argc+1 words.
     */

    /* NOTE(khim): we are using ILP32 model in x86-64 mode! */
    leal    4(%rsp, %rsi, 4), %ebx

    /*
     * Align the stack 0mod16, for SSE2
     */
    andq    $0xfffffffffffffff0, %rsp

    /*
     *Save the arguments in spare registers.
     */
    movq    %rsi, %rdi
    movq    %rcx, %rsi
    movl    %ebx, %edx
    call    __nacl_startup

halt_loop:
    hlt
    jmp     halt_loop
    .p2align 5

