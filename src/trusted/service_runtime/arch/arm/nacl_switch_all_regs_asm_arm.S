/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"


/*
 * The register range we use in 'ldm' below is only possible in the
 * original-ARM instruction set, not in Thumb2.
 */
.arm

/* void NaClSwitchAllRegsAsm(struct NaClSignalContext *regs); */
DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClSwitchAllRegsAsm):
        /* Restore CPSR (flags) register, a.k.a. ASPR for user mode */
        ldr r1, [r0, #0x40]
        msr apsr_nzcvqg, r1

        ldmia r0, {r0-pc}
