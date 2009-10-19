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

/*
 * NaCl service runtime context switch code.  NaClSyscallSeg is the
 * lcall target from the syscall trampoline code, and this code is
 * responsible for figuring out the identity of the thread, saving
 * the user registers, finish restoring the segment registers (and
 * getting out of the sandbox), and actually invoking the C system
 * call handler code.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

/*
 * on stack:
 *
 *  syscall-arg-N
 *  ...
 *  syscall-arg-1
 *  RA for caller to libc stub
 *  code seg from trampoline lcall
 *  RA for trampoline lcall (used to compute syscall number)
 */

.globl  IDENTIFIER(NaClSyscallSeg)
        HIDDEN(NaClSyscallSeg)
IDENTIFIER(NaClSyscallSeg):
        /*
         * Visual Studio "protects" the stack in compiler generated prolog
         * using an unprotected rep prefix (the /GS option):

  0040115F: 8D BD AC FD FF FF  lea         edi,[ebp-254h]
  00401165: B9 8F 00 00 00     mov         ecx,8Fh
  0040116A: B8 CC CC CC CC     mov         eax,0CCCCCCCCh
  0040116F: F3 AB              rep stos    dword ptr es:[edi]

         * which actually overwrites the previous stack frame if the D flag
         * is set.  Ironic that stack smashing protection enables a stack
         * smashing attack.
         *
         * There are other uses that assume D is unset, for example,
         * inlined structure copies:

  00419190: 8B 75 0C           mov         esi,dword ptr [ebp+0Ch]
  00419193: 8B 7D 08           mov         edi,dword ptr [ebp+8]
  00419196: 83 C7 0C           add         edi,0Ch
  00419199: B9 07 00 00 00     mov         ecx,7
  0041919E: F3 A5              rep movs    dword ptr es:[edi],dword ptr [esi]

         *
         * As far as we know, glibc-derived code always explicitly clears
         * (or sets) the direction flag, so this is an issue only for Windows.
         */

        cld
        xor     %eax, %eax
        mov     %gs, %ax
        shr     $3, %eax
        lea     IDENTIFIER(nacl_user)(,%eax,4), %edx

        movl    0(%edx), %edx
        movl    %ebx, 0x0(%edx)
        movl    %esi, 0x4(%edx)
        movl    %edi, 0x8(%edx)
        movl    %ebp, 0x14(%edx)
        movl    %esp, 0x1c(%edx)

        /*
         * we do not save the registers (below) that the user is powerless
         * to change.  eip will be overwritten w/ the validated return
         * address pulled from the stack for the syscall return.
         *
         * these segment registers are restored by NaCl_springboard for
         * syscall return.
         */

#if 0
        movl    %eip, 0xc(%edx)
        mov     %cs, 0x3c(%edx)
        mov     %ds, 0x28(%edx)
        mov     %es, 0x2a(%edx)
        mov     %fs, 0x2c(%edx)
        mov     %gs, 0x2e(%edx)
        mov     %ss, 0x20(%edx)
#endif
        /*
         * load only the system segments; called code do not depend on
         * any registers having particular values.  we will clear/discard
         * caller-saved registers at system call return.
         */
        lea     IDENTIFIER(nacl_sys)(,%eax,4), %edx
        movl    0(%edx), %edx
        /*
         * %cs and %ds already taken care of by NaCl_trampoline_seg_code
         *
         * %ebx need not be saved/restored even if -fpic were used, since
         * in that case %ebx is initialized on fn entry.
         */
        mov     0x2a(%edx), %es
        mov     0x2c(%edx), %fs
        mov     0x2e(%edx), %gs

        mov     0x20(%edx), %ecx  /* ss */
        movl    0x1c(%edx), %edx  /* esp */
        mov     %ecx, %ss  /* disable intr */
        movl    %edx, %esp
        push    %eax
        call    IDENTIFIER(NaClSyscallCSegHook)
        /*
         * If stack usage in the above code changes, modify initial %esp
         * computation -- see nacl_switch_to_app.c:NaClStartThreadInApp.
         */
        hlt
        /* noret */
