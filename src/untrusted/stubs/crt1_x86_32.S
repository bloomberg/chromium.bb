/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* Native Client crt0 startup code for x86-64 */

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
     * The ABI uses a null frame pointer to say when to stop backtracing.
     * In x86-64 case we don't need this because RBP handling is special
     * and so loader will load proper value in RBP.
     */

    xorl    %ebp, %ebp

    /*
     *  Because we are going to align the stack 0mod16 for SSE2,
     *  We need to gather the argc, argv, and envp pointers before
     *  moving esp.
     */
    popl    %esi               /* Remove argc from the top of the stack */
    movl    %esp, %ecx         /* Save the argv pointer */

    /*
     * Finding envp requires skipping over argc+1 words.
     */
    leal    4(%esp, %esi, 4), %ebx

    /*
     * Align the stack 0mod16, for SSE2
     */
    andl    $0xfffffff0, %esp

    /*
     * Push the arguments to main.
     */
    pushl   %ebp  /* Padding to maintain 0mod16 alignment */
    pushl   %ebx  /* Push envp onto the stack */
    pushl   %ecx  /* Push argv onto the stack */
    pushl   %esi  /* Push argc back onto the stack */

    call    __nacl_startup

halt_loop:
    hlt
    jmp     halt_loop
    .p2align 5

