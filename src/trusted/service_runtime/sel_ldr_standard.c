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
 * NaCl Simple/secure ELF loader (NaCl SEL).
 */

#include "native_client/src/include/portability.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/include/elf_constants.h"
#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_time.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"

#include "native_client/src/trusted/service_runtime/arch/sel_ldr_arch.h"
#include "native_client/src/trusted/service_runtime/elf_util.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_check.h"
#include "native_client/src/trusted/service_runtime/nacl_closure.h"
#include "native_client/src/trusted/service_runtime/nacl_sync_queue.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"
#include "native_client/src/trusted/service_runtime/tramp.h"

#define PTR_ALIGN_MASK  ((sizeof(void *))-1)


NaClErrorCode NaClAppLoadFile(struct Gio       *gp,
                              struct NaClApp   *nap,
                              enum NaClAbiCheckOption check_abi) {
  NaClErrorCode        ret = LOAD_INTERNAL;
  NaClErrorCode        subret;
  uintptr_t            max_vaddr;
  struct NaClElfImage  *image = NULL;
  /* NACL_MAX_ADDR_BITS < 32 */
  if (nap->addr_bits > NACL_MAX_ADDR_BITS) {
    ret = LOAD_ADDR_SPACE_TOO_BIG;
    goto done;
  }

  nap->stack_size = NaClRoundAllocPage(nap->stack_size);

  /* temporay object will be deleted at end of function */
  image = NaClElfImageNew(gp);
#if !defined(DANGEROUS_DEBUG_MODE_DISABLE_INNER_SANDBOX)
  check_abi = NACL_ABI_CHECK_OPTION_CHECK;
#endif

  if (check_abi == NACL_ABI_CHECK_OPTION_CHECK) {
    subret = NaClElfImageValidateAbi(image);
    if (subret != LOAD_OK) {
      ret = subret;
      goto done;
    }

  }

  subret = NaClElfImageValidateElfHeader(image);
  if (subret != LOAD_OK) {
    ret = subret;
    goto done;
  }

  subret = NaClElfImageValidateProgramHeaders(image,
                                              nap->addr_bits,
                                              &nap->text_region_bytes,
                                              &max_vaddr);
  if (subret != LOAD_OK) {
    ret = subret;
    goto done;
  }

  nap->break_addr = max_vaddr;
  nap->data_end = max_vaddr;

  nap->align_boundary = NaClElfImageGetAlignBoundary(image);
  if (nap->align_boundary == 0) {
    ret = LOAD_BAD_ABI;
    goto done;
  }

  nap->entry_pt = NaClElfImageGetEntryPoint(image);

  NaClLog(2,
          "text_region_bytes: %08x  "
          "break_add: %08"PRIxPTR"  "
          "data_end: %08"PRIxPTR"  "
          "entry_pt: %08x  "
          "align_boundary: %08x\n",
          nap->text_region_bytes,
          nap->break_addr,
          nap->data_end,
          nap->entry_pt,
          nap->align_boundary);
  if (!NaClAddrIsValidEntryPt(nap, nap->entry_pt)) {
    ret = LOAD_BAD_ENTRY;
    goto done;
  }

  NaClLog(2, "Allocating address space\n");
  subret = NaClAllocAddrSpace(nap);
  if (subret != LOAD_OK) {
    ret = subret;
    goto done;
  }

  NaClLog(2, "Loading into memory\n");
  subret = NaClElfImageLoad(image, gp, nap->addr_bits, nap->mem_start);
  if (subret != LOAD_OK) {
    ret = subret;
    goto done;
  }

  NaClFillEndOfTextRegion(nap);

#if !defined(DANGEROUS_DEBUG_MODE_DISABLE_INNER_SANDBOX)
  NaClLog(2, "Validating image\n");
  subret = NaClValidateImage(nap);
  if (subret != LOAD_OK) {
    ret = subret;
    goto done;
  }
#endif

  NaClLog(2, "Installing trampoline\n");

  NaClLoadTrampoline(nap);

  /*
   * This is necessary for ARM arch as we keep TLS pointer in register r9 or in
   * nacl_tls_idx varible. TLS hook is a piece of code patched into a trampoline
   * slot to retrieve the TLS pointer when needed by nacl module. The hook is
   * very simple piece of code, so we do not go to service runtime to perform
   * it.
   */
  NaClLoadTlsHook(nap);

  NaClLog(2, "Installing springboard\n");

  NaClLoadSpringboard(nap);

  NaClLog(2, "Applying memory protection\n");

  subret = NaClMemoryProtection(nap);
  if (subret != LOAD_OK) {
    ret = subret;
    goto done;
  }

  ret = LOAD_OK;
done:
  NaClElfImageDelete(image);
  return ret;
}

int NaClAddrIsValidEntryPt(struct NaClApp *nap,
                           uintptr_t      addr) {
  if (0 != (addr & (nap->align_boundary - 1))) {
    return 0;
  }

  return addr < NACL_TRAMPOLINE_END + nap->text_region_bytes;
}

/*
 * preconditions:
 * argc > 0, argc and argv table is consistent
 * envv may be NULL (this happens on MacOS/Cocoa
 * if envv is non-NULL it is 'consistent', null terminated etc.
 */
int NaClCreateMainThread(struct NaClApp     *nap,
                         int                argc,
                         char               **argv,
                         char const *const  *envv) {
  /*
   * Compute size of string tables for argv and envv
   */
  int                   retval;
  int                   envc;
  char const *const     *pp;
  size_t                size;
  int                   i;
  char                  *p;
  char                  *strp;
  int                   *argv_len;
  int                   *envv_len;
  struct NaClAppThread  *natp;
  uintptr_t             stack_ptr;

  /*
   * Set an exception handler so Windows won't show a message box if
   * an exception occurs
   */
  WINDOWS_EXCEPTION_TRY;

  retval = 0;  /* fail */
  CHECK(argc > 0);
  CHECK(NULL != argv);

  if (envv == NULL) {
    envc = 0;
  } else {
    for (pp = envv, envc = 0; NULL != *pp; ++pp, ++envc)
      ;
  }
  envv_len = 0;
  argv_len = malloc(argc * sizeof argv_len[0]);
  envv_len = malloc(envc * sizeof envv_len[0]);
  if (NULL == argv_len) {
    goto cleanup;
  }
  if (NULL == envv_len && 0 != envc) {
    goto cleanup;
  }

  size = 0;

  for (i = 0; i < argc; ++i) {
    argv_len[i] = strlen(argv[i]) + 1;
    size += argv_len[i];
  }
  for (i = 0; i < envc; ++i) {
    envv_len[i] = strlen(envv[i]) + 1;
    size += envv_len[i];
  }

  size += (argc + envc + 4) * sizeof(char *) + sizeof(int);

  size = (size + PTR_ALIGN_MASK) & ~PTR_ALIGN_MASK;

  if (size > nap->stack_size) {
    retval = 0;
    goto cleanup;
  }

  /* write strings and char * arrays to stack */

  stack_ptr = (nap->mem_start + (1 << nap->addr_bits) - size);
  VCHECK(0 == (stack_ptr & PTR_ALIGN_MASK),
          ("stack_ptr not aligned: %08x\n", stack_ptr));

  p = (char *) stack_ptr;
  strp = p + (argc + envc + 4) * sizeof(char *) + sizeof(int);

#define BLAT(t, v) do { \
    *(t *) p = (t) v; p += sizeof(t);           \
  } while (0);

  BLAT(int, argc);

  for (i = 0; i < argc; ++i) {
    BLAT(char *, NaClSysToUser(nap, (uintptr_t) strp));
    strcpy(strp, argv[i]);
    strp += argv_len[i];
  }
  BLAT(char *, 0);

  for (i = 0; i < envc; ++i) {
    BLAT(char *, NaClSysToUser(nap, (uintptr_t) strp));
    strcpy(strp, envv[i]);
    strp += envv_len[i];
  }
  BLAT(char *, 0);
  /* Push an empty auxv for glibc support */
  BLAT(char *, 0);
  BLAT(char *, 0);
#undef BLAT

  /* now actually spawn the thread */
  natp = malloc(sizeof *natp);
  if (!natp) {
    goto cleanup;
  }

  nap->running = 1;

  /* e_entry is user addr */
  if (!NaClAppThreadAllocSegCtor(natp,
                                 nap,
                                 1,
                                 nap->entry_pt,
                                 NaClSysToUser(nap, stack_ptr),
                                 NaClUserToSys(nap, nap->break_addr),
                                 1)) {
    retval = 0;
    goto cleanup;
  }

  /*
   * NB: Hereafter locking is required to access nap.
   */
  retval = 1;
cleanup:
  free(argv_len);
  free(envv_len);

  WINDOWS_EXCEPTION_CATCH;
  return retval;
}

int NaClWaitForMainThreadToExit(struct NaClApp  *nap) {
  struct NaClClosure        *work;

  while (NULL != (work = NaClSyncQueueDequeue(&nap->work_queue))) {
    NaClLog(3, "NaClWaitForMainThreadToExit: got work %08"PRIxPTR"\n",
            (uintptr_t) work);
    NaClLog(3, " invoking Run fn %08"PRIxPTR"\n",
            (uintptr_t) work->vtbl->Run);

    (*work->vtbl->Run)(work);
    NaClLog(3, "... done\n");
  }

  NaClLog(3, " taking NaClApp lock\n");
  NaClXMutexLock(&nap->mu);
  NaClLog(3, " waiting for exit status\n");
  while (nap->running) {
    NaClCondVarWait(&nap->cv, &nap->mu);
  }
  /*
   * Some thread invoked the exit (exit_group) syscall.
   */

  return (nap->exit_status);
}

int32_t NaClCreateAdditionalThread(struct NaClApp *nap,
                                   uintptr_t      prog_ctr,
                                   uintptr_t      stack_ptr,
                                   uintptr_t      sys_tdb,
                                   size_t         tdb_size) {
  struct NaClAppThread  *natp;

  natp = malloc(sizeof *natp);
  if (NULL == natp) {
    return -NACL_ABI_ENOMEM;
  }
  if (!NaClAppThreadAllocSegCtor(natp,
                                 nap,
                                 0,
                                 prog_ctr,
                                 stack_ptr,
                                 sys_tdb,
                                 tdb_size)) {
    return -NACL_ABI_ENOMEM;
  }
  return 0;
}
