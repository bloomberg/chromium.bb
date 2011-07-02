/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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
#elif NACL_LINUX || NACL_OSX
        /* elif Linux/OSX, 1st param is already in %rdi. */
        mov     %rdi, %rcx
#else
# error "What OS/compiler is the service runtime being compiled with?"
#endif

        movq    0x8(%rcx), %rbx
        movq    0x20(%rcx), %rbp
        movq    0x60(%rcx), %r12
        movq    0x68(%rcx), %r13
        movq    0x70(%rcx), %r14
        movq    0x78(%rcx), %r15

        /* there is no springboard for x86_64 */
        movq    0x38(%rcx), %rsp  /* rsp -- switch stack */
        movq    0x90(%rcx), %rax  /* syscall return */

        /*
         * %rdi is the first argument in the user calling convention.
         * When starting the initial thread, we are passing the address
         * of the parameter block here.  The initial stack pointer has
         * been adjusted to one word below there, to insert a dummy
         * return address for the user entry point function.
         */
        leal    8(%rsp), %edi

        /*
         * Zero all unused registers.  The 32-bit instructions
         * are a byte shorter than their 64-bit counterparts
         * when the target register is one of the first eight,
         * and they implicitly zero the high halves.
         */
        xorl    %edx, %edx
        movl    %edx, %esi
        movq    %rdx, %r8
        movq    %rdx, %r9
        movq    %rdx, %r10
        movq    %rdx, %r11

        jmp     *0x88(%rcx)
        /*
         * This leaves %rcx pointing to the thread context, but that
         * should be memory that's inaccessible by NaCl module code.
         */
