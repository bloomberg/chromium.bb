/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Test trampoline to test thread capture mitigation.  Trusted code
 * thunk injected into NaCl module only with NACL_FAULT_INJECTION, so
 * should not appear in normal execution.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

        .text
DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClTestCapture):
#if NACL_LINUX
        movq    nacl_thread_index@GOTTPOFF(%rip), %rax
        movl    $0, %fs:(%rax)
#elif NACL_OSX
        movl    _nacl_thread_index_tls_offset(%rip), %eax
        movl    $0, %gs:(%rax)
#elif NACL_WINDOWS
        .byte 0xba
        .secrel32 nacl_thread_index

        mov     _tls_index(%rip), %ecx
        movq    %gs:0x58, %rax
        movq    (%rax,%rcx,8), %rax
        movl    $0, (%rdx,%rax)
#else
# error "What OS/compiler is the service runtime being compiled with?"
#endif
        /* sandbox the return, straight off the untrusted stack */
        movq    0x8(%rsp), %rcx
        addq    $0x10, %rsp
        addl    $31, %ecx
        andl    $0xffffffe0, %ecx
        addq    %r15, %rcx
        /*
         * Set %rdx to zero so that we do not leak nacl_tls[thread_id]
         * address.  This also resets flags.  Resetting flags to fixed
         * values by doing this last simplifies testing that we do not
         * leak any information via flags.
         */
        xorl    %edx, %edx
        jmp     *%rcx
DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClTestCaptureEnd):
