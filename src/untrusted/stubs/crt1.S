/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Native Client crt0 startup code */

        .data
        .globl environ

        .text
        .p2align NACLENTRYALIGN,0xf4,

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
         * environ is initiallly set to point to the same location as envp.
         * setenv, etc., may change this pointer later.
         */
        movl    %ebx, environ

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

        /*
         * Install the fini section for use at exit.  The C++ static object
         * destructors are invoked from here.
         */
        subl    $12, %esp  /* Padding to maintain 0mod16 alignment */
        pushl   $_fini
        call    atexit
        addl    $16, %esp  /* Pop parameter and padding */

        /*
         * Initialize the pthreads library.  We need to do at least a minimal
         * amount of initialization (e.g., set up gs) to allow thread local
         * storage references to work.  The default binding of the symbol
         * is weak, replaced by the real pthread library initialization when
         * present.
         */
        call    __pthread_initialize

        /*
         * Install the pthread_shutdown call to be called at exit.
         */
        subl    $12, %esp  /* Padding to maintain 0mod16 alignment */
        pushl   $__pthread_shutdown
        call    atexit
        addl    $16, %esp  /* Pop parameter and padding */

        /*
         * Execute the init section before starting main.  The C++ static
         * object constructors are invoked from here.
         */
        call    _init

        /*
         * Initialize the SRPC module before starting main.  There is a weak
         * definition in libnacl that can be overridden by libsrpc.
         */
        call    __srpc_init

        /*
         * Wait for libav startup to connect to the browser.  There is a weak
         * definition in libnacl that can be overridden by libav.
         */
        call    __av_wait

        /*
         * Invoke main, the start of the user's code.
         */
        call    main
        subl    $12, %esp  /* Make space for the return value */
        pushl   %eax       /* Save return value for use by exit call. */

        /*
         * Wait for srpc shutdown.  There is a weak definition in libnacl
         * that can be overridden by libsrpc.
         */
        call    __srpc_wait

        /*
         * Call exit from the C library so atexit gets called, and the
         * C++ destructors get run. This calls our exit routine below
         * when it's done.
         */
        call    exit
        addl    $32, %esp  /* Clean up all the arguments */

halt_loop:
        hlt
        jmp     halt_loop

