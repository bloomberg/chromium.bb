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
  ldr  r3, .L.gNaClThreadIdxPIC
.LPIC0:
  add  r3, pc
  ldr  r3, [r3]
  /*
   * Fetch the thread-local variable holding the thread index.
   * This funny instruction is what the compiler generates for -mtp=cp15,
   * which is the default for ARMv7 code generation.
   */
  mrc  p15, 0, r0, c13, c0, 3
  mov  r1, #0
  str  r1, [r0, r3]
  pop  {ip}
  pop  {r0, r1, r2, r3}
  bx   ip

  .balign 4
.L.gNaClThreadIdxPIC:
  .word  gNaClThreadIdx(gottpoff) + (. - .LPIC0 - 8)
