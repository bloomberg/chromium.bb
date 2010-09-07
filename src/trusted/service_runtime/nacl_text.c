/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <string.h>

#include "native_client/src/include/concurrency_ops.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_effector.h"
#include "native_client/src/trusted/desc/nacl_desc_effector.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/service_runtime/arch/sel_ldr_arch.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/mman.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "native_client/src/trusted/service_runtime/nacl_text.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"

/*
 * Private subclass of NaClDescEffector, used only in this file.
 */
struct NaClDescEffectorShm {
  struct NaClDescEffector   base;
};

static
void NaClDescEffectorShmDtor(struct NaClDescEffector *vself) {
  /* no base class dtor to invoke */

  vself->vtbl = (struct NaClDescEffectorVtbl *) NULL;

  return;
}

static
int NaClDescEffectorShmUnmapMemory(struct NaClDescEffector  *vself,
                                   uintptr_t                sysaddr,
                                   size_t                   nbytes) {
  UNREFERENCED_PARAMETER(vself);
  /*
   * Existing memory is anonymous paging file backed.
   */
  NaClLog(4, "NaClDescEffectorShmUnmapMemory called\n");
  NaClLog(4, " sysaddr 0x%08"NACL_PRIxPTR", "
          "0x%08"NACL_PRIxS" (%"NACL_PRIdS")\n",
          sysaddr, nbytes, nbytes);
  NaCl_page_free((void *) sysaddr, nbytes);
  return 0;
}

static
uintptr_t NaClDescEffectorShmMapAnonymousMemory(struct NaClDescEffector *vself,
                                                uintptr_t               sysaddr,
                                                size_t                  nbytes,
                                                int                     prot) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(sysaddr);
  UNREFERENCED_PARAMETER(nbytes);
  UNREFERENCED_PARAMETER(prot);

  NaClLog(LOG_FATAL, "NaClDescEffectorShmMapAnonymousMemory called\n");
  /* NOTREACHED but gcc doesn't know that */
  return -NACL_ABI_EINVAL;
}

static
struct NaClDescEffectorVtbl kNaClDescEffectorShmVtbl = {
  NaClDescEffectorShmDtor,
  NaClDescEffectorShmUnmapMemory,
  NaClDescEffectorShmMapAnonymousMemory,
};

int NaClDescEffectorShmCtor(struct NaClDescEffectorShm *self) {
  self->base.vtbl = &kNaClDescEffectorShmVtbl;
  return 1;
}

NaClErrorCode NaClMakeDynamicTextShared(struct NaClApp *nap) {
  enum NaClErrorCode          retval = LOAD_INTERNAL;
  uintptr_t                   dynamic_text_size;
  struct NaClDescImcShm       *shm = NULL;
  struct NaClDescEffectorShm  shm_effector;
  int                         shm_effector_initialized = 0;
  uintptr_t                   shm_vaddr_base;
  uintptr_t                   shm_offset;
  uintptr_t                   mmap_ret;

  uintptr_t                   shm_upper_bound;

  shm_vaddr_base = NaClEndOfStaticText(nap);
  NaClLog(4,
          "NaClMakeDynamicTextShared: shm_vaddr_base = %08"NACL_PRIxPTR"\n",
          shm_vaddr_base);
  shm_vaddr_base = NaClRoundAllocPage(shm_vaddr_base);
  NaClLog(4,
          "NaClMakeDynamicTextShared: shm_vaddr_base = %08"NACL_PRIxPTR"\n",
          shm_vaddr_base);

  if (!nap->use_shm_for_dynamic_text) {
    NaClLog(4,
            "NaClMakeDynamicTextShared:"
            "  rodata / data segments not allocation aligned\n");
    NaClLog(4,
            " not using shm for text\n");
    nap->dynamic_text_start = shm_vaddr_base;
    nap->dynamic_text_end = shm_vaddr_base;
    return LOAD_OK;
  }

  /*
   * Allocate a shm region the size of which is nap->rodata_start -
   * end-of-text.  This implies that the "core" text will not be
   * backed by shm.
   */
  shm_upper_bound = nap->rodata_start;
  if (0 == shm_upper_bound) {
    shm_upper_bound = nap->data_start;
  }
  if (0 == shm_upper_bound) {
    shm_upper_bound = shm_vaddr_base;
  }
  nap->dynamic_text_start = shm_vaddr_base;
  nap->dynamic_text_end = shm_upper_bound;

  NaClLog(4, "shm_upper_bound = %08"NACL_PRIxPTR"\n", shm_upper_bound);

  dynamic_text_size = shm_upper_bound - shm_vaddr_base;
  NaClLog(4,
          "NaClMakeDynamicTextShared: dynamic_text_size = %"NACL_PRIxPTR"\n",
          dynamic_text_size);

  if (0 == dynamic_text_size) {
    NaClLog(4, "Empty JITtable region\n");
    return LOAD_OK;
  }

  shm = (struct NaClDescImcShm *) malloc(sizeof *shm);
  if (NULL == shm) {
    goto cleanup;
  }
  if (!NaClDescImcShmAllocCtor(shm, dynamic_text_size)) {
    /* cleanup invariant is if ptr is non-NULL, it's fully ctor'd */
    free(shm);
    shm = NULL;
    NaClLog(4, "NaClMakeDynamicTextShared: shm creation for text failed\n");
    retval = LOAD_NO_MEMORY;
    goto cleanup;
  }
  if (!NaClDescEffectorShmCtor(&shm_effector)) {
    NaClLog(4,
            "NaClMakeDynamicTextShared: shm effector"
            " initialization failed\n");
    retval = LOAD_INTERNAL;
    goto cleanup;
  }
  shm_effector_initialized = 1;

  /*
   * Map shm in place of text.  We currently do this eagerly, which
   * can result in excess memory/swap traffic.
   *
   * TODO(bsy): consider using NX and doing this lazily, or mapping a
   * canonical HLT-filled 64K shm repeatedly, and only remapping with
   * a "real" shm text as needed.
   */
  for (shm_offset = 0;
       shm_offset < dynamic_text_size;
       shm_offset += NACL_MAP_PAGESIZE) {
    uintptr_t text_vaddr = shm_vaddr_base + shm_offset;
    uintptr_t text_sysaddr = NaClUserToSys(nap, text_vaddr);

    NaClLog(4,
            "NaClMakeDynamicTextShared: Map(,,0x%"NACL_PRIxPTR",size = 0x%x,"
            " prot=0x%x, flags=0x%x, offset=0x%"NACL_PRIxPTR"\n",
            text_sysaddr,
            NACL_MAP_PAGESIZE,
            NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
            NACL_ABI_MAP_SHARED | NACL_ABI_MAP_FIXED,
            shm_offset);
    mmap_ret = (*((struct NaClDescVtbl const *)
                  shm->base.base.vtbl)->
                Map)((struct NaClDesc *) shm,
                     (struct NaClDescEffector *) &shm_effector,
                     (void *) text_sysaddr,
                     NACL_MAP_PAGESIZE,
                     NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
                     NACL_ABI_MAP_SHARED | NACL_ABI_MAP_FIXED,
                     shm_offset);
    if (text_sysaddr != mmap_ret) {
      NaClLog(LOG_FATAL,
              "Could not map in shm for dynamic text region, HLT filling.\n");
    }
    NaClFillMemoryRegionWithHalt((void *) text_sysaddr, NACL_MAP_PAGESIZE);
    if (-1 == (*((struct NaClDescVtbl const *)
                 shm->base.base.vtbl)->
               UnmapUnsafe)((struct NaClDesc *) shm,
                            ((struct NaClDescEffector *)
                             &shm_effector),
                            (void *) text_sysaddr,
                            NACL_MAP_PAGESIZE)) {
      NaClLog(LOG_FATAL,
              "Could not unmap shm for dynamic text region, post HLT fill.\n");
    }
    NaClLog(4,
            "NaClMakeDynamicTextShared: Map(,,0x%"NACL_PRIxPTR",size = 0x%x,"
            " prot=0x%x, flags=0x%x, offset=0x%"NACL_PRIxPTR"\n",
            text_sysaddr,
            NACL_MAP_PAGESIZE,
            NACL_ABI_PROT_READ | NACL_ABI_PROT_EXEC,
            NACL_ABI_MAP_SHARED | NACL_ABI_MAP_FIXED,
            shm_offset);
    mmap_ret = (*((struct NaClDescVtbl const *)
                  shm->base.base.vtbl)->
                Map)((struct NaClDesc *) shm,
                     (struct NaClDescEffector *) &shm_effector,
                     (void *) text_sysaddr,
                     NACL_MAP_PAGESIZE,
                     NACL_ABI_PROT_READ | NACL_ABI_PROT_EXEC,
                     NACL_ABI_MAP_SHARED | NACL_ABI_MAP_FIXED,
                     shm_offset);
    if (text_sysaddr != mmap_ret) {
      NaClLog(LOG_FATAL, "Could not map in shm for dynamic text region\n");
    }
  }

  nap->text_shm = &shm->base;
  retval = LOAD_OK;

cleanup:
  if (shm_effector_initialized) {
    (*shm_effector.base.vtbl->Dtor)((struct NaClDescEffector *) &shm_effector);
  }
  if (LOAD_OK != retval) {
    NaClDescSafeUnref((struct NaClDesc *) shm);
    free(shm);
  }

  return retval;
}

/*
 * Since we do not currently keep track of code insertion buffers to
 * enforce that code deletion will delete inserted regions atomically,
 * we use regions filled with the HLT instruction as a simple way to
 * detect that the dynamic code region is currently unused by any
 * previous code insertion operation.  Note that we must forbid
 * insertion of regions that contain bundles which are HLT filled,
 * since otherwise this check can be fooled.
 */
static int CodeRangeIsUnused(uint8_t *data, uint32_t size) {
  uint32_t *end = (uint32_t *) (data + size);
  uint32_t *addr;
  for (addr = (uint32_t *) data; addr < end; addr++) {
    /* Check that the code range contains only HLT instructions. */
    if (NACL_HALT_WORD != *addr)
      return 0;
  }
  return 1;
}

static void CopyBundleTails(uint8_t *dest,
                            uint8_t *src,
                            int32_t size,
                            int     bundle_size) {
  /*
   * The order in which these locations are written does not matter:
   * none of the locations will be reachable, because the bundle heads
   * still contains HLTs.
   */
  int       bundle_mask = bundle_size - 1;
  uint32_t  *src_ptr;
  uint32_t  *dest_ptr;
  uint32_t  *end_ptr;

  CHECK(0 == ((uintptr_t) dest & 3));

  src_ptr = (uint32_t *) src;
  dest_ptr = (uint32_t *) dest;
  end_ptr = (uint32_t *) (dest + size);
  while (dest_ptr < end_ptr) {
    if ((((uintptr_t) dest_ptr) & bundle_mask) != 0) {
      *dest_ptr = *src_ptr;
    }
    dest_ptr++;
    src_ptr++;
  }
}

static void CopyBundleHeads(uint8_t  *dest,
                            uint8_t  *src,
                            uint32_t size,
                            int      bundle_size) {
  /* Again, the order in which these locations are written does not matter. */
  uint8_t *src_ptr;
  uint8_t *dest_ptr;
  uint8_t *end_ptr;

  /* dest must be aligned for the writes to be atomic. */
  CHECK(0 == ((uintptr_t) dest & 3));

  src_ptr = src;
  dest_ptr = dest;
  end_ptr = dest + size;
  while (dest_ptr < end_ptr) {
    /*
     * We assume that writing the 32-bit int here is atomic, which is
     * the case on x86 and ARM as long as the address is word-aligned.
     * The read does not have to be atomic.
     */
    *(uint32_t *) dest_ptr = *(uint32_t *) src_ptr;
    dest_ptr += bundle_size;
    src_ptr += bundle_size;
  }
}

static void CopyCodeSafely(uint8_t  *dest,
                           uint8_t  *src,
                           uint32_t size,
                           int      bundle_size) {
  CopyBundleTails(dest, src, size, bundle_size);
  NaClWriteMemoryBarrier();
  CopyBundleHeads(dest, src, size, bundle_size);
  /*
   * For security, we are fine; for correctness, another memory
   * barrier is needed here.  We assume that there will be one later
   * in the syscall processing and/or in the inter-thread
   * communication needed for the code inserting thread to tell other
   * threads that the newly inserted code is ready to use (and at what
   * address it lives).
   */
}

/*
 * Assuming the nbytes bytes of code at code_buf starts a new bundle,
 * verify that no bundles in the buffer contain only HLT instructions.
 * nbytes must be an integer multiple of nap->bundle_size.
 */
static int NaClCodeContainsUnusedMarker(struct NaClApp  *nap,
                                        uint8_t         *code_buf,
                                        uint32_t        nbytes) {
  uint32_t  *p = (uint32_t *) code_buf;
  uint32_t  *p_end = (uint32_t *) (code_buf + nbytes);
  uint32_t  words_per_bundle = nap->bundle_size / sizeof(uint32_t);
  uint32_t  ix;
  int       bundle_ok;

  while (p < p_end) {
    bundle_ok = 0;
    for (ix = 0; ix < words_per_bundle; ++ix) {
      if (p[ix] != NACL_HALT_WORD) {
        bundle_ok = 1;
        break;
      }
    }
    if (!bundle_ok) {
      NaClLog(1,
              ("NaClCodeContainsUnusedMarker: found bad bundle at byte"
               " offset 0x%"NACL_PRIx32" from start of bundle.\n"),
              (uint32_t) ((uint8_t *) p - code_buf));
      return 1;
    }
    p += words_per_bundle;
  }

  return 0;
}

int32_t NaClTextSysDyncode_Copy(struct NaClAppThread *natp,
                                uint32_t             dest,
                                uint32_t             src,
                                uint32_t             size) {
  struct NaClApp              *nap = natp->nap;
  uintptr_t                   dest_addr;
  uintptr_t                   src_addr;
  uint32_t                    shm_offset;
  uint32_t                    shm_map_offset;
  uint32_t                    within_page_offset;
  uint32_t                    shm_map_offset_end;
  uint32_t                    shm_map_size;
  struct NaClDescEffectorShm  shm_effector;
  struct NaClDesc             *shm = nap->text_shm;
  uintptr_t                   mmap_ret;
  uint8_t                     *mmap_result;
  uint8_t                     *mapped_addr;
  uint8_t                     *code_copy;
  int                         validator_result;
  int32_t                     retval = -NACL_ABI_EINVAL;

  if (!shm) {
    NaClLog(1, "NaClTextSysDyncode_Copy: Dynamic loading not enabled\n");
    return -NACL_ABI_EINVAL;
  }
  if (0 != (dest & (nap->bundle_size - 1)) ||
      0 != (size & (nap->bundle_size - 1))) {
    NaClLog(1, "NaClTextSysDyncode_Copy: Non-bundle-aligned address or size\n");
    return -NACL_ABI_EINVAL;
  }
  dest_addr = NaClUserToSysAddrRange(nap, dest, size);
  src_addr = NaClUserToSysAddrRange(nap, src, size);
  if (kNaClBadAddress == dest_addr ||
      kNaClBadAddress == src_addr) {
    NaClLog(1, "NaClTextSysDyncode_Copy: Address out of range\n");
    return -NACL_ABI_EFAULT;
  }
  if (dest < nap->dynamic_text_start) {
    NaClLog(1, "NaClTextSysDyncode_Copy: Below dynamic code area\n");
    return -NACL_ABI_EFAULT;
  }
  /*
   * We ensure that the final HLTs of the dynamic code region cannot
   * be overwritten, just in case of CPU bugs.
   */
  if (dest + size > nap->dynamic_text_end - NACL_HALT_SLED_SIZE) {
    NaClLog(1, "NaClTextSysDyncode_Copy: Above dynamic code area\n");
    return -NACL_ABI_EFAULT;
  }
  if (size == 0) {
    /* Nothing to load.  Succeed trivially. */
    return 0;
  }

  shm_offset = dest - (uint32_t) nap->dynamic_text_start;
  shm_map_offset = shm_offset & ~(NACL_MAP_PAGESIZE - 1);
  within_page_offset = shm_offset & (NACL_MAP_PAGESIZE - 1);
  shm_map_offset_end =
    (shm_offset + size + NACL_MAP_PAGESIZE - 1) & ~(NACL_MAP_PAGESIZE - 1);
  shm_map_size = shm_map_offset_end - shm_map_offset;

  if (!NaClDescEffectorShmCtor(&shm_effector)) {
    NaClLog(LOG_FATAL,
            "NaClTextSysDyncode_Copy: "
            "shm effector initialization failed\n");
  }

  mmap_ret = (*((struct NaClDescVtbl const *)
                shm->base.vtbl)->
              Map)(shm, (struct NaClDescEffector *) &shm_effector,
                   NULL, shm_map_size,
                   NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
                   NACL_ABI_MAP_SHARED, shm_map_offset);
  if (NaClIsNegErrno(mmap_ret)) {
    return (int32_t) mmap_ret;
  }
  mmap_result = (uint8_t *) mmap_ret;
  mapped_addr = mmap_result + within_page_offset;
  CHECK(mmap_result <= mapped_addr);
  CHECK(mapped_addr + size <= mmap_result + shm_map_size);

  /*
   * Make a private copy of the code, so that we can validate it
   * without a TOCTTOU race condition.
   */
  code_copy = malloc(size);
  if (!code_copy) {
    retval = -NACL_ABI_ENOMEM;
    goto cleanup_unmap;
  }
  memcpy(code_copy, (uint8_t *) src_addr, size);

  if (NaClCodeContainsUnusedMarker(nap, code_copy, size)) {
    retval = -NACL_ABI_EINVAL;
    goto cleanup_free;
  }

  NaClMutexLock(&nap->dynamic_load_mutex);

  /*
   * In principle, this can go outside the lock, but the validator
   * has global mutable state which might not be used thread-safely.
   * TODO(mseaborn): Check before moving this outside the lock.
   */
  validator_result = NaClValidateCode(nap, dest, code_copy, size);

  if (validator_result != LOAD_OK && nap->ignore_validator_result) {
    NaClLog(LOG_ERROR, "VALIDATION FAILED for dynamically-loaded code: "
            "continuing anyway...\n");
    validator_result = LOAD_OK;
  }

  if (validator_result != LOAD_OK) {
    NaClLog(1, "NaClTextSysDyncode_Copy: "
            "Validation of dynamic code failed\n");
    retval = -NACL_ABI_EINVAL;
    goto cleanup_unlock;
  }

  if (!CodeRangeIsUnused(mapped_addr, size)) {
    if (!nap->allow_dyncode_replacement) {
        /*
         * We cannot safely overwrite this memory because other
         * threads could be executing code from it.
         */
        NaClLog(1, "NaClTextSysDyncode_Copy: Code range already allocated\n");
        retval = -NACL_ABI_EINVAL;
        goto cleanup_unlock;
    }

    /*
     * target addr is in use... lets see if this is a valid replacement
     */
    validator_result = NaClValidateCodeReplacement(nap, dest,
                                                   mapped_addr, code_copy,
                                                   size);

    if (validator_result != LOAD_OK && nap->ignore_validator_result) {
      NaClLog(LOG_ERROR, "VALIDATION FAILED for dynamically-loaded code: "
              "continuing anyway...\n");
      validator_result = LOAD_OK;
    }

    if (validator_result != LOAD_OK) {
      NaClLog(1, "NaClTextSysDyncode_Copy: (REWRITE) "
              "Validation of dynamic code failed\n");

      retval = -NACL_ABI_EINVAL;
      goto cleanup_unlock;
    }

    /*
     * TODO(jansel): this is NOT thread safe, we must dock all threads
     *               before replacing code
     */
    memcpy(mapped_addr, code_copy, size);
    retval = 0;
  } else {
    CopyCodeSafely(mapped_addr, code_copy, size, nap->bundle_size);
    retval = 0;
  }

cleanup_unlock:
  NaClMutexUnlock(&nap->dynamic_load_mutex);

cleanup_free:
  free(code_copy);

cleanup_unmap:
  if (0 != (*((struct NaClDescVtbl const *)
              shm->base.vtbl)->
            UnmapUnsafe)(shm,
                         (struct NaClDescEffector *) &shm_effector,
                         mmap_result, shm_map_size)) {
    NaClLog(LOG_FATAL, "NaClTextSysDyncode_Copy: Failed to unmap\n");
  }
  return retval;
}
