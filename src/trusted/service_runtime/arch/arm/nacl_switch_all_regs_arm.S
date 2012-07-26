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

/*
 * void NaClSwitchAllRegs(struct NaClAppThread *natp,
 *                        struct NaClSignalContext *regs);
 */
DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClSwitchAllRegs):
        ldmia r1, {r0-pc}
