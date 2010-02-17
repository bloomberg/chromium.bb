/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

.text

/*
 * NOTE: this code is likely not used. Most places call _aeabi_read_tp()
 *       in the src/untrusted/stubs/crt1_arm.S instead.
 * TODO(robertm): maybe delete the code
 *
 * Assembly code template.
 * This is linked into the service runtime but is unused as code -- it is used
 * as data to be patched into a NaCl app's address space.
 *
 * This code is patched into trampoline region, in the pre-last slot (the last
 * slot is occupied by the springboard). This code is used by the nacl module
 * for obtaining a pointer to its TLS region. A function __aeabi_read_tp in nacl
 * libc must be tweaked to jump to the trampoline slot with this code.
 *
 * r9 keeps the TLS_IDX which is concatenation of a tdb pointer and index in
 * nacl_thread/nacl_user/nacl_sys arrays. The higher 20 bits are for tdb
 * address and the lower 12 keep the index.
 * The first word of the tdb points to the tls are for the thread.
 *
 * This code is copied into nacl module address space by
 * sel_ldr_arm::NaClLoadTlsHook()
 * The target location is just below  NACL_TRAMPOLINE_END
 */

DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClReadTP_start):

  /*
   * NOTE: A copy of this code also exists in the arm startup under
   *       the arm eabi name  __aeabi_read_tp()
   */
  mov r0, r9, lsr #NACL_PAGESHIFT
  lsl r0, #NACL_PAGESHIFT
#if 0 == NACL_DANGEROUS_DEBUG_MODE_DISABLE_INNER_SANDBOX
  bic lr, lr, #NACL_CONTROL_FLOW_MASK
#endif
  bx lr

DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaClReadTP_end):
