/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NaCl Secure Runtime
 */

#include "native_client/src/trusted/desc/nacl_desc_effector_ldr.h"


uint16_t  nacl_global_cs = 0;
uint16_t  nacl_global_ds = 0;


void NaClInitGlobals() {
  nacl_global_cs = NaClGetCs();
  nacl_global_ds = NaClGetDs();
}

uint16_t NaClGetGlobalDs() {
  return nacl_global_ds;
}

uint16_t NaClGetGlobalCs() {
  return nacl_global_cs;
}

int NaClThreadContextCtor(struct NaClThreadContext  *ntcp,
                          struct NaClApp            *nap,
                          uintptr_t                 prog_ctr,
                          uintptr_t                 stack_ptr,
                          uint32_t                  tls_idx) {
  NaClLog(4, "&nap->code_seg_sel = 0x%08"PRIxPTR"\n",
          (uintptr_t) &nap->code_seg_sel);
  NaClLog(4, "&nap->data_seg_sel = 0x%08"PRIxPTR"\n",
          (uintptr_t) &nap->data_seg_sel);
  NaClLog(4, "nap->code_seg_sel = 0x%02x\n", nap->code_seg_sel);
  NaClLog(4, "nap->data_seg_sel = 0x%02x\n", nap->data_seg_sel);

  /*
   * initialize registers to appropriate values.  most registers just
   * get zero, but for the segment register we allocate segment
   * selectors for the NaCl app, based on its address space.
   */
  ntcp->ebx = 0;
  ntcp->esi = 0;
  ntcp->edi = 0;
  ntcp->frame_ptr = 0;
  ntcp->stack_ptr = stack_ptr;
  ntcp->prog_ctr = prog_ctr;

  ntcp->cs = nap->code_seg_sel;
  ntcp->ds = nap->data_seg_sel;

  ntcp->es = nap->data_seg_sel;
  ntcp->fs = 0;  /* windows use this for TLS and SEH; linux does not */
  ntcp->gs = (uint16_t)tls_idx;
  ntcp->ss = nap->data_seg_sel;

  NaClLog(4, "user.cs: 0x%02x\n", ntcp->cs);
  NaClLog(4, "user.fs: 0x%02x\n", ntcp->fs);
  NaClLog(4, "user.gs: 0x%02x\n", ntcp->gs);
  NaClLog(4, "user.ss: 0x%02x\n", ntcp->ss);

  return 1;
}
