/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Simple/secure ELF loader (NaCl SEL).
 */

#ifndef SERVICE_RUNTIME_TRAMP_H__
#define SERVICE_RUNTIME_TRAMP_H__ 1
/*
 * text addresses, not word aligned; these are .globl symbols from the
 * assembler source, and there are no type information associated with
 * them.  we could declare these to be (void (*)(void)), i suppose,
 * but it doesn't really matter since we convert their addresses to
 * uintptr_t at every use.
 */
extern char   NaCl_trampoline_code, NaCl_trampoline_end;
extern char   NaCl_tramp_patch;
extern char   NaCl_trampoline_seg_code, NaCl_trampoline_seg_end;
extern char   NaCl_tramp_cseg_patch, NaCl_tramp_dseg_patch;
extern char   NaCl_text_prot, NaCl_text_prot_end;
#endif
