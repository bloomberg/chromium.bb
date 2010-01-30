/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * This code gets executed when switching from the 64-bit service
 * runtime to a 32-bit nacl module. NaClSwitch has one parameter only,
 * which is a struct passed by reference.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

        /*
         * This function does not return.  Thus, we need to preserve
         * any callee-saved registers.
         */

        .text
.globl  IDENTIFIER(NaClSwitch)
        HIDDEN(NaClSwitch)
IDENTIFIER(NaClSwitch):
#if NACL_WINDOWS
        /* if Windows, 1st param is already in %rcx, not %rdi */
        mov     %rcx, %rdi
#elif NACL_LINUX || NACL_OSX
        /* elif Linux/OSX, 1st param is already in %rdi. */
#else
# error "What OS/compiler is the service runtime being compiled with?"
#endif

        movq    0x8(%rdi), %rbx
        movq    0x20(%rdi), %rbp
        movq    0x60(%rdi), %r12
        movq    0x68(%rdi), %r13
        movq    0x70(%rdi), %r14
        movq    0x78(%rdi), %r15

        /* there is no springboard for x86_64 */
        movq    0x38(%rdi), %rsp  /* rsp -- switch stack */
        movq    0x90(%rdi), %rax  /* syscall return */
        jmp     *0x88(%rdi)
        /*
         * This leaves %rdi pointing to the thread context, but that
         * should be memory that's inaccessible by NaCl module code.
         */
