/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * The ELF entry point ABI is such that assembly code is required to
 * call the entry point.
 * See http://code.google.com/p/nativeclient/issues/detail?id=1131
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

        /*
         * arg 1:  r0, new stack pointer
         * arg 2:  r1, entry point
         * arg 3:  r2, atexit function, to be left in r0
         */
        .global jump_to_elf_start
jump_to_elf_start:
        mov sp, r0
        bic sp, sp, #NACL_DATA_FLOW_MASK

        mov r0, r2
        nop

        bic r1, r1, #NACL_CONTROL_FLOW_MASK
        bx r1
