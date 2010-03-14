/*
 * Copyright 2008 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_time.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"

#include "native_client/src/trusted/service_runtime/arch/sel_ldr_arch.h"
#include "native_client/src/trusted/service_runtime/elf_util.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_closure.h"
#include "native_client/src/trusted/service_runtime/nacl_sync_queue.h"
#include "native_client/src/trusted/service_runtime/nacl_text.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"

#if !defined(SIZE_T_MAX)
# define SIZE_T_MAX     (~(size_t) 0)
#endif


NaClErrorCode NaClAppLoadFile(struct Gio       *gp,
                              struct NaClApp   *nap,
                              enum NaClAbiCheckOption check_abi) {
  NaClErrorCode       ret = LOAD_INTERNAL;
  NaClErrorCode       subret;
  uintptr_t           rodata_end;
  uintptr_t           data_end;
  uintptr_t           max_vaddr;
  struct NaClElfImage *image = NULL;

  /* NACL_MAX_ADDR_BITS < 32 */
  if (nap->addr_bits > NACL_MAX_ADDR_BITS) {
    ret = LOAD_ADDR_SPACE_TOO_BIG;
    goto done;
  }

  nap->stack_size = NaClRoundAllocPage(nap->stack_size);

  /* temporay object will be deleted at end of function */
  image = NaClElfImageNew(gp, &subret);
  if (NULL == image) {
    NaClLog(LOG_FATAL,
            "Could not create NaClElfImage object: %s\n",
            NaClErrorString(subret));
  }

#if 0 == NACL_DANGEROUS_DEBUG_MODE_DISABLE_INNER_SANDBOX
  check_abi = NACL_ABI_CHECK_OPTION_CHECK;
#endif

  if (NACL_ABI_CHECK_OPTION_CHECK == check_abi) {
    subret = NaClElfImageValidateAbi(image);
    if (subret != LOAD_OK) {
      ret = subret;
      goto done;
    }
  }

  subret = NaClElfImageValidateElfHeader(image);
  if (LOAD_OK != subret) {
    ret = subret;
    goto done;
  }

  subret = NaClElfImageValidateProgramHeaders(image,
                                              nap->addr_bits,
                                              &nap->static_text_end,
                                              &nap->rodata_start,
                                              &rodata_end,
                                              &nap->data_start,
                                              &data_end,
                                              &max_vaddr);
  if (LOAD_OK != subret) {
    ret = subret;
    goto done;
  }

  nap->break_addr = max_vaddr;
  nap->data_end = max_vaddr;

  NaClLog(4, "rodata_end = %08"NACL_PRIxPTR"\n", rodata_end);
  NaClLog(4, "data_start = %08"NACL_PRIxPTR"\n", nap->data_start);
  NaClLog(4, "data_end   = %08"NACL_PRIxPTR"\n", data_end);
  NaClLog(4, "max_vaddr  = %08"NACL_PRIxPTR"\n", max_vaddr);

#if 0 == NACL_DANGEROUS_DEBUG_MODE_DISABLE_INNER_SANDBOX
  nap->bundle_size = NaClElfImageGetBundleSize(image);
  if (nap->bundle_size == 0) {
    ret = LOAD_BAD_ABI;
    goto done;
  }
#else
  /* pick some reasonable default for an un-sandboxed nexe */
  nap->bundle_size = 32;
#endif
  nap->entry_pt = NaClElfImageGetEntryPoint(image);

  NaClLog(2,
          "static_text_end: 0x%016"NACL_PRIxPTR"  "
          "break_add: 0x%016"NACL_PRIxPTR"  "
          "data_end: 0x%016"NACL_PRIxPTR"  "
          "entry_pt: 0x%016"NACL_PRIxPTR"  "
          "bundle_size: 0x%x\n",
          nap->static_text_end,
          nap->break_addr,
          nap->data_end,
          nap->entry_pt,
          nap->bundle_size);

  if (!NaClAddrIsValidEntryPt(nap, nap->entry_pt)) {
    ret = LOAD_BAD_ENTRY;
    goto done;
  }

  /*
   * Basic address space layout sanity check.
   */
  if (0 != nap->data_start) {
    if (data_end != max_vaddr) {
      NaClLog(LOG_INFO, "data segment is not last\n");
      ret = LOAD_DATA_NOT_LAST_SEGMENT;
      goto done;
    }
  } else if (0 != nap->rodata_start) {
    if (rodata_end != max_vaddr) {
      /*
       * This should be unreachable, but we include it just for
       * completeness.
       *
       * Here is why it is unreachable:
       *
       * NaClPhdrChecks checks the test segment starting address.  The
       * only allowed loaded segments are text, data, and rodata.
       * Thus unless the rodata is in the trampoline region, it must
       * be after the text.  And NaClElfImageValidateProgramHeaders
       * ensures that all segments start after the trampoline region.
       */
      NaClLog(LOG_INFO, "no data segment, but rodata segment is not last\n");
      ret = LOAD_NO_DATA_BUT_RODATA_NOT_LAST_SEGMENT;
      goto done;
    }
  }
  if (0 != nap->rodata_start && 0 != nap->data_start) {
    if (rodata_end > nap->data_start) {
      NaClLog(LOG_INFO, "rodata_overlaps data.\n");
      ret = LOAD_RODATA_OVERLAPS_DATA;
      goto done;
    }
  }
  if (0 != nap->rodata_start) {
    if (NaClRoundAllocPage(NaClEndOfStaticText(nap)) > nap->rodata_start) {
      ret = LOAD_TEXT_OVERLAPS_RODATA;
      goto done;
    }
  } else if (0 != nap->data_start) {
    if (NaClRoundAllocPage(NaClEndOfStaticText(nap)) > nap->data_start) {
      ret = LOAD_TEXT_OVERLAPS_DATA;
      goto done;
    }
  }

  NaClLog(2, "Allocating address space\n");
  subret = NaClAllocAddrSpace(nap);
  if (LOAD_OK != subret) {
    ret = subret;
    goto done;
  }

  /*
   * NB: mem_map object has been initialized, but is empty.
   * NaClMakeDynamicTextShared does not touch it.
   *
   * NaClMakeDynamicTextShared also fills the dynamic memory region
   * with the architecture-specific halt instruction.  If/when we use
   * memory mapping to save paging space for the dynamic region and
   * lazily halt fill the memory as the pages become
   * readable/executable, we must make sure that the *last*
   * NACL_MAP_PAGESIZE chunk is nonetheless mapped and written with
   * halts.
   */
  NaClLog(2,
          ("Replacing gap between static text and"
           " (ro)data with shareable memory\n"));
  subret = NaClMakeDynamicTextShared(nap);
  if (LOAD_OK != subret) {
    ret = subret;
    goto done;
  }

  NaClLog(2, "Loading into memory\n");
  subret = NaClElfImageLoad(image, gp, nap->addr_bits, nap->mem_start);
  if (LOAD_OK != subret) {
    ret = subret;
    goto done;
  }

  /*
   * NaClFillEndOfTextRegion will fill with halt instructions the
   * padding space after the static text region.
   *
   * Shm-backed dynamic text space was filled with halt instructions
   * in NaClMakeDynamicTextShared.  This extends to the rodata.  For
   * non-shm-backed text space, this extend to the next page (and not
   * allocation page).  static_text_end is updated to include the
   * padding.
   */
  NaClFillEndOfTextRegion(nap);

#if 0 == NACL_DANGEROUS_DEBUG_MODE_DISABLE_INNER_SANDBOX
  NaClLog(2, "Validating image\n");
  subret = NaClValidateImage(nap);
  if (LOAD_OK != subret) {
    ret = subret;
    goto done;
  }
#endif

  NaClLog(2, "Installing trampoline\n");

  NaClLoadTrampoline(nap);

  NaClLog(2, "Installing springboard\n");

  NaClLoadSpringboard(nap);

  NaClLog(2, "Applying memory protection\n");

  /*
   * NaClMemoryProtect also initializes the mem_map w/ information
   * about the memory pages and their current protection value.
   *
   * The contents of the dynamic text region will get remapped as
   * non-writable.
   */
  subret = NaClMemoryProtection(nap);
  if (LOAD_OK != subret) {
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
  if (0 != (addr & (nap->bundle_size - 1))) {
    return 0;
  }

  return addr < nap->static_text_end;
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
  size_t                ptr_tbl_size;
  int                   i;
  char                  *p;
  char                  *strp;
  size_t                *argv_len;
  size_t                *envv_len;
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

  if (NULL == envv) {
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

  /*
   * The following two loops cannot overflow.  The reason for this is
   * that they are counting the number of bytes used to hold the
   * NUL-terminated strings that comprise the argv and envv tables.
   * If the entire address space consisted of just those strings, then
   * the size variable would overflow; however, since there's the code
   * space required to hold the code below (and we are not targetting
   * Harvard architecture machines), at least one page holds code, not
   * data.  We are assuming that the caller is non-adversarial and the
   * code does not look like string data....
   */
  for (i = 0; i < argc; ++i) {
    argv_len[i] = strlen(argv[i]) + 1;
    size += argv_len[i];
  }
  for (i = 0; i < envc; ++i) {
    envv_len[i] = strlen(envv[i]) + 1;
    size += envv_len[i];
  }

  /*
   * NaCl modules are ILP32, so the argv, envv pointers, as well as
   * the terminating NULL pointers at the end of the argv/envv tables,
   * are 32-bit values.  We also have the empty auxv to take into
   * account, so that's 6 additional 32-bit values on top of the
   * argv/envv contents.  Note that on nacl64, argc is popped, and
   * is an 8-byte value!
   *
   * The argv and envv pointer tables came from trusted code and is
   * part of memory.  Thus, by the same argument above, adding in
   * (argc+envc+4)*sizeof(void *) cannot possibly overflow the size
   * variable since it is a size_t object.  However, the two more
   * pointers for auxv and the space for argv could cause an overflow.
   * The fact that we used stack to get here etc means that
   * ptr_tb_size could not have overflowed.
   *
   * NB: the underlying OS would have limited the amount of space used
   * for argv and envv -- on linux, it is ARG_MAX, or 128KB -- and
   * hence the overflow check is for obvious auditability rather than
   * for correctness.
   */
  ptr_tbl_size = (argc + envc + 6) * sizeof(uint32_t) + sizeof(int);

  if (SIZE_T_MAX - size < ptr_tbl_size) {
    NaClLog(LOG_WARNING,
            "NaClCreateMainThread: ptr_tb_size cause size of"
            " argv / environment copy to overflow!?!\n");
    retval = 0;
    goto cleanup;
  }
  size += ptr_tbl_size;

  size = (size + NACL_STACK_ALIGN_MASK) & ~NACL_STACK_ALIGN_MASK;

  if (size > nap->stack_size) {
    retval = 0;
    goto cleanup;
  }

  /* write strings and char * arrays to stack */
  stack_ptr = (nap->mem_start + ((uintptr_t) 1U << nap->addr_bits) - size);

  NaClLog(2, "setting stack to : %016"NACL_PRIxPTR"\n", stack_ptr);

  VCHECK(0 == (stack_ptr & NACL_STACK_ALIGN_MASK),
         ("stack_ptr not aligned: %016"NACL_PRIxPTR"\n", stack_ptr));

  p = (char *) stack_ptr;
  strp = p + ptr_tbl_size;

#define BLAT(t, v) do { \
    *(t *) p = (t) v; p += sizeof(t);           \
  } while (0);

  BLAT(nacl_reg_t, argc);

  for (i = 0; i < argc; ++i) {
    BLAT(uint32_t, NaClSysToUser(nap, (uintptr_t) strp));
    NaClLog(2, "copying arg %d  %p -> %p\n",
            i, argv[i], strp);
    strcpy(strp, argv[i]);
    strp += argv_len[i];
  }
  BLAT(uint32_t, 0);

  for (i = 0; i < envc; ++i) {
    BLAT(uint32_t, NaClSysToUser(nap, (uintptr_t) strp));
    NaClLog(2, "copying env %d  %p -> %p\n",
            i, envv[i], strp);
    strcpy(strp, envv[i]);
    strp += envv_len[i];
  }
  BLAT(uint32_t, 0);
  /* Push an empty auxv for glibc support */
  BLAT(uint32_t, 0);
  BLAT(uint32_t, 0);
#undef BLAT

  /* now actually spawn the thread */
  natp = malloc(sizeof *natp);
  if (!natp) {
    goto cleanup;
  }

  nap->running = 1;

  NaClLog(2, "system stack ptr : %016"NACL_PRIxPTR"\n", stack_ptr);
  NaClLog(2, "  user stack ptr : %016"NACL_PRIxPTR"\n",
          NaClSysToUserStackAddr(nap, stack_ptr));

  /* e_entry is user addr */
  if (!NaClAppThreadAllocSegCtor(natp,
                                 nap,
                                 1,
                                 nap->entry_pt,
                                 NaClSysToUserStackAddr(nap, stack_ptr),
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
    NaClLog(3, "NaClWaitForMainThreadToExit: got work %08"NACL_PRIxPTR"\n",
            (uintptr_t) work);
    NaClLog(3, " invoking Run fn %08"NACL_PRIxPTR"\n",
            (uintptr_t) work->vtbl->Run);

    (*work->vtbl->Run)(work);
    NaClLog(3, "... done\n");
  }

  NaClLog(3, " taking NaClApp lock\n");
  NaClXMutexLock(&nap->mu);
  NaClLog(3, " waiting for exit status\n");
  while (nap->running) {
    NaClCondVarWait(&nap->cv, &nap->mu);
    NaClLog(3, " wakeup, nap->running %d, nap->exit_status %d\n",
            nap->running, nap->exit_status);
  }
  NaClXMutexUnlock(&nap->mu);
  /*
   * Some thread invoked the exit (exit_group) syscall.
   */

  return (nap->exit_status);
}

/*
 * stack_ptr is from syscall, so a 32-bit address.
 */
int32_t NaClCreateAdditionalThread(struct NaClApp *nap,
                                   uintptr_t      prog_ctr,
                                   uintptr_t      sys_stack_ptr,
                                   uintptr_t      sys_tdb,
                                   size_t         tdb_size) {
  struct NaClAppThread  *natp;
  uintptr_t             stack_ptr;

  natp = malloc(sizeof *natp);
  if (NULL == natp) {
    NaClLog(LOG_WARNING,
            ("NaClCreateAdditionalThread: no memory for new thread context."
             "  Returning EAGAIN per POSIX specs.\n"));
    return -NACL_ABI_EAGAIN;
  }

  stack_ptr = NaClSysToUserStackAddr(nap, sys_stack_ptr);

  if (0 != ((stack_ptr + sizeof(nacl_reg_t)) & NACL_STACK_ALIGN_MASK)) {
    NaClLog(LOG_WARNING,
            ("NaClCreateAdditionalThread:  user thread library"
             " did not provide properly aligned user stack pointer\n"));
    NaClLog(LOG_WARNING,
            "NaClCreateAdditionalThread:  orig stack ptr: 0x%"NACL_PRIxPTR"\n",
            stack_ptr);

    stack_ptr &= ~NACL_STACK_ALIGN_MASK;
    stack_ptr -= sizeof(nacl_reg_t);  /* pretend return addr */

    NaClLog(LOG_WARNING,
            "NaClCreateAdditionalThread: fixed stack ptr: 0x%"NACL_PRIxPTR"\n",
            stack_ptr);
  }

  if (!NaClAppThreadAllocSegCtor(natp,
                                 nap,
                                 0,
                                 prog_ctr,
                                 stack_ptr,
                                 sys_tdb,
                                 tdb_size)) {
    NaClLog(LOG_WARNING,
            ("NaClCreateAdditionalThread: could not allocate thread index."
             "  Returning EAGAIN per POSIX specs.\n"));
    free(natp);
    return -NACL_ABI_EAGAIN;
  }
  return 0;
}
