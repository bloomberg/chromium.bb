; Copyright (c) 2012 The Native Client Authors. All rights reserved.
; Use of this source code is governed by a BSD-style license that can be
; found in the LICENSE file.

include ksamd64.inc

PUBLIC NaClSwitchSavingStackPtr

_TEXT SEGMENT

; This routine passes control on to NaClSwitch(), after setting up a
; trusted stack that will later be used for running
; NaClSyscallCSegHook().  The stack is set up so that it can be
; unwound, which involves storing a return address with unwind info.
; To generate unwind info, we need to use Microsoft's x86-64
; assembler.
;
; We want to make the stack unwindable so that a last-chance exception
; handler registered with SetUnhandledExceptionFilter() -- in
; particular, Breakpad's handler -- will get called when a hardware
; exception occurs.  x86-64 Windows uses stack unwinding to find
; exception handlers, and the last-chance handler is not guaranteed to
; get called if the stack is malformed.  For background, see:
; http://code.google.com/p/nativeclient/issues/detail?id=2237
; http://code.google.com/p/nativeclient/issues/detail?id=2414

; Arg 1 (rcx):  user_context (&natp->user), passed on to NaClSwitch()
; Arg 2 (rdx):  address of natp->user.trusted_stack_ptr
; Arg 3 (r8):  address of CPUID-specific NaClSwitch() function
NaClSwitchSavingStackPtr PROC FRAME
        ; Save all callee-saved registers.  This is necessary in case
        ; the caller is using a frame pointer.  The Windows ABI
        ; permits the caller to use any callee-saved register as a
        ; frame pointer, not just rbp, so we have to save them all.
        push_reg rbp
        push_reg r12
        push_reg r13
        push_reg r14
        push_reg r15
        push_reg rdi
        push_reg rsi
        push_reg rbx
        ; Allocate 0x20 bytes for the shadow space, plus an additional
        ; 8 bytes to align the stack pointer.
        alloc_stack(28h)
.endprolog
        ; Save rsp-8 in struct NaClThreadContext.
        ; i.e. Save the value of rsp as it will appear after the
        ; return address is pushed by the 'call' instruction.
        mov   r12, rsp
        sub   r12, 8
        mov   [rdx], r12

        call  r8
        hlt
NaClSwitchSavingStackPtr ENDP

_TEXT ENDS

END
