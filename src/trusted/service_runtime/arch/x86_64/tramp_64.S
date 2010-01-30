/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Simple/secure ELF loader (NaCl SEL).
 */

/*
 * NaCl code uses the standard x86-64 calling convention, whereas the
 * service runtime might be using the x86-64 calling convention or the
 * Microsoft x86-64 calling convention.  See arch/x86_64/sel_rt_64.h
 * for a discussion and pointers to references.
 *
 * Available registers: rax, rcx, rdx, r8, r9, r10, r11, since these
 * are all caller saved.  r15 is nominally callee saved, but for NaCl
 * code r15 is rzp, the reserved zero-address base pointer for the
 * start of the NaCl module's address space.  Since its value is
 * easily regenerated, we can trash it here.
 *
 * Arguments: rdi, rsi, rdx, rcx, r8, r9 (standard x86-64; rest on stack)
 *            rcx, rdx, r8, r9 (Microsoft; rest on stack)
 *
 * This means that we will need to conditionalize the trampoline code
 * based on which compiler was used for the service_runtime.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

        .text
        .globl IDENTIFIER(NaCl_trampoline_code)
HIDDEN(NaCl_trampoline_code)
IDENTIFIER(NaCl_trampoline_code):
        mov $0xdeadbeefcafebabe, %rax
        .globl IDENTIFIER(NaCl_trampoline_call_target)
HIDDEN(NaCl_trampoline_call_target)
IDENTIFIER(NaCl_trampoline_call_target):
        call *%rax
        /* call IDENTIFIER(NaClSyscallSeg) */
        hlt
        .globl IDENTIFIER(NaCl_trampoline_code_end)
HIDDEN(NaCl_trampoline_code_end)
IDENTIFIER(NaCl_trampoline_code_end):
