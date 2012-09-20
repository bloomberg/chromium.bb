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
        movw $0x4e61, %ax  /* placeholder, replaced when patched */
DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClTestCaptureGs):
        movw %ax, %gs
        pop %eax
        andl $~0x1f, %eax
        jmp *%eax
DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClTestCaptureEnd):
