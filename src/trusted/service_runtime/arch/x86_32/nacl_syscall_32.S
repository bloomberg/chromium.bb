/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl service runtime context switch code.  NaClSyscallSeg is the
 * lcall target from the syscall trampoline code, and this code is
 * responsible for figuring out the identity of the thread, saving
 * the user registers, finish restoring the segment registers (and
 * getting out of the sandbox), and actually invoking the C system
 * call handler code.
 */

#include "native_client/src/trusted/service_runtime/arch/x86_32/sel_rt_32.h"
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

        .text

        /*
         * In the PIC case:
         *
         * The trampoline code calls NaClPcrelThunk using an lcall,
         * restoring %cs.  Next, NaClPcrelThunk invokes
         * NaClSyscallSeg, with nacl_user in %ecx and %ds restored.
         * The other segment registers remains to be restored, and we
         * use address differences to obtain nacl_sys (and any other
         * global variables), avoiding PIC address synthesis using the
         * usual sequence of
         *
         *     call 0f
         * 0:  pop %eax
         *
         * to get the address of the pop into a register.  Such
         * sequences are verboten in the context switch code, since we
         * are still using a stack in untrusted memory, and we need to
         * figure out the per-thread secure stack first.
         */
DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClSyscallSeg):
        cld
        xor     %eax, %eax
        movw    %gs, %ax
        shr     $3, %eax

#if __PIC__
        movl    0(%ecx), %edx
        movl    0(%edx,%eax,4), %edx
#else
        movl    IDENTIFIER(nacl_user)(,%eax,4), %edx
#endif

        /* check NaClThreadContext in sel_rt_32.h for the offsets */
        movl    %ebx, NACL_THREAD_CONTEXT_OFFSET_EBX(%edx)
        movl    %esi, NACL_THREAD_CONTEXT_OFFSET_ESI(%edx)
        movl    %edi, NACL_THREAD_CONTEXT_OFFSET_EDI(%edx)
        movl    %ebp, NACL_THREAD_CONTEXT_OFFSET_FRAME_PTR(%edx)
        movl    %esp, NACL_THREAD_CONTEXT_OFFSET_STACK_PTR(%edx)

        /*
         * Save the x87 FPU control word.  This is callee-saved,
         * while all other x87 state is caller-saved.  Then reload
         * the system default state to use while running trusted code.
         */
        fnstcw  NACL_THREAD_CONTEXT_OFFSET_FCW(%edx)
        fldcw   NACL_THREAD_CONTEXT_OFFSET_SYS_FCW(%edx)

        /*
         * We do not save segment registers, which untrusted code is
         * powerless to change.
         */

        /*
         * load only the system segments; called code do not depend on
         * any registers having particular values.  we will clear/discard
         * caller-saved registers at system call return.
         */
#if __PIC__
        movl    4(%ecx), %edx
        movl    0(%edx,%eax,4), %edx
#else
        movl    IDENTIFIER(nacl_sys)(,%eax,4), %edx
#endif
        /*
         * %cs and %ds already taken care of by NaCl_trampoline_seg_code
         *
         * %ebx need not be saved/restored even if -fpic were used, since
         * in that case %ebx is initialized on fn entry.
         */
        movw    NACL_THREAD_CONTEXT_OFFSET_ES(%edx), %es
        movw    NACL_THREAD_CONTEXT_OFFSET_FS(%edx), %fs
        movw    NACL_THREAD_CONTEXT_OFFSET_GS(%edx), %gs

        /* 'lss' sets %ss as well as %esp */
        lss     NACL_THREAD_CONTEXT_OFFSET_STACK_PTR(%edx), %esp
        push    %eax
        call    IDENTIFIER(NaClSyscallCSegHook)
        /*
         * If stack usage in the above code changes, modify initial %esp
         * computation -- see nacl_switch_to_app.c:NaClStartThreadInApp.
         */
        hlt
        /* noret */
