/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <float.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_rt.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"


uintptr_t NaClGetThreadCtxSp(struct NaClThreadContext  *th_ctx) {
  return (uintptr_t) th_ctx->stack_ptr.ptr_32.ptr;
}


void NaClSetThreadCtxSp(struct NaClThreadContext  *th_ctx, uintptr_t sp) {
  th_ctx->stack_ptr.ptr_32.ptr = (uint32_t) sp;
}


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
                          nacl_reg_t                prog_ctr,
                          nacl_reg_t                stack_ptr,
                          nacl_reg_t                tls_idx) {
  NaClLog(4, "&nap->code_seg_sel = 0x%08"NACL_PRIxPTR"\n",
          (uintptr_t) &nap->code_seg_sel);
  NaClLog(4, "&nap->data_seg_sel = 0x%08"NACL_PRIxPTR"\n",
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
#if NACL_BUILD_SUBARCH == 64
  ntcp->frame_ptr.ptr_64 = 0;
  ntcp->stack_ptr.ptr_64 = stack_ptr;
#else
  ntcp->frame_ptr.ptr_32.ptr = 0;
  ntcp->stack_ptr.ptr_32.ptr = stack_ptr;
#endif
  ntcp->prog_ctr = prog_ctr;
  ntcp->new_prog_ctr = 0;
  ntcp->sysret = -NACL_ABI_EINVAL;

  ntcp->cs = nap->code_seg_sel;
  ntcp->ds = nap->data_seg_sel;

  ntcp->es = nap->data_seg_sel;
  ntcp->fs = 0;  /* windows use this for TLS and SEH; linux does not */
  ntcp->gs = (uint16_t) tls_idx;
  ntcp->ss = nap->data_seg_sel;

  ntcp->fcw = NACL_X87_FCW_DEFAULT;

  /*
   * Save the system's state of the x87 FPU control word so we can restore
   * the same state when returning to trusted code.
   */
#if NACL_WINDOWS
  ntcp->sys_fcw = _control87(0, 0);
#else
  __asm__ __volatile__("fnstcw %0" : "=m" (ntcp->sys_fcw));
#endif

  NaClLog(4, "user.cs: 0x%02x\n", ntcp->cs);
  NaClLog(4, "user.fs: 0x%02x\n", ntcp->fs);
  NaClLog(4, "user.gs: 0x%02x\n", ntcp->gs);
  NaClLog(4, "user.ss: 0x%02x\n", ntcp->ss);

  return 1;
}
