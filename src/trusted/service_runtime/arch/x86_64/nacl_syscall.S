/*
 * Copyright 2009, Google Inc.
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

/*
 * This code gets executed when switching from a 32-bit nacl module to
 * the 64-bit service. NaClSyscallSeg is the lcall target from the
 * syscall trampoline code, and this code is responsible for figuring
 * out the identity of the thread, saving the user registers, finish
 * restoring the segment registers (and getting out of the sandbox),
 * and actually invoking the C system call handler code.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

.globl  IDENTIFIER(NaClSyscallSeg)
        HIDDEN(NaClSyscallSeg)
IDENTIFIER(NaClSyscallSeg):
        /*
         * As far as we know, glibc-derived code always explicitly clears
         * (or sets) the direction flag, so this is an issue only for Windows.
         */

        cld
        xor     %eax, %eax
        mov     %gs, %ax
        shr     $3, %eax
        lea     IDENTIFIER(nacl_user)(,%eax,4), %edx

        /* check NaClThreadContext in sel_rt.h for the offsets */
        movl    0(%edx), %edx
        movl    %ebx, 0x0(%edx)
        movl    %esi, 0x4(%edx)
        movl    %edi, 0x8(%edx)
        movl    %ebp, 0x14(%edx)
        mov     %rsp, 0x18(%edx)

        /*
         * load only the system segments; called code do not depend on
         * any registers having particular values.  we will clear/discard
         * caller-saved registers at system call return.
         */
        lea     IDENTIFIER(nacl_sys)(,%eax,4), %edx
        mov     0(%edx), %edx
        /*
         * %cs and %ds already taken care of by NaCl_trampoline_seg_code
         *
         * %ebx need not be saved/restored even if -fpic were used, since
         * in that case %ebx is initialized on fn entry.
         */
        mov     0x2c(%edx), %fs
        mov     0x2e(%edx), %gs

        mov     0x18(%edx), %edx  /* sp */
        mov     %ecx, %ss  /* disable intr */
        mov     %rdx, %rsp
        push    %rax
        call    IDENTIFIER(NaClSyscallCSegHook)
        /*
         * If stack usage in the above code changes, modify initial %esp
         * computation -- see nacl_switch_to_app.c:NaClStartThreadInApp.
         */
        hlt
        /* noret */
