/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_effector.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/mman.h"
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
int NaClDescEffectorShmReturnCreatedDesc(struct NaClDescEffector  *vself,
                                         struct NaClDesc          *ndp) {
  UNREFERENCED_PARAMETER(vself);
  UNREFERENCED_PARAMETER(ndp);

  NaClLog(LOG_FATAL, "NaClDescEffectorShmReturnCreatedDesc called\n");
  return -NACL_ABI_EINVAL;
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
struct NaClDescImcBoundDesc *NaClDescEffectorShmSourceSock(
    struct NaClDescEffector *vself) {
  UNREFERENCED_PARAMETER(vself);

  NaClLog(LOG_FATAL, "NaClDescEffectorShmSourceSock called\n");
  /* NOTREACHED but gcc doesn't know that */
  return NULL;
}

static
struct NaClDescEffectorVtbl kNaClDescEffectorShmVtbl = {
  NaClDescEffectorShmDtor,
  NaClDescEffectorShmReturnCreatedDesc,
  NaClDescEffectorShmUnmapMemory,
  NaClDescEffectorShmMapAnonymousMemory,
  NaClDescEffectorShmSourceSock,
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

  if (!nap->use_shm_for_dynamic_text) {
    NaClLog(4,
            "NaClMakeDynamicTextShared:"
            "  rodata / data segments not allocation aligned\n");
    NaClLog(4,
            " not using shm for text\n");
    return LOAD_OK;
  }

  /*
   * Allocate a shm region the size of which is nap->rodata_start -
   * end-of-text.  This implies that the "core" text will not be
   * backed by shm.
   */
  shm_vaddr_base = NaClEndOfStaticText(nap);
  NaClLog(4,
          "NaClMakeDynamicTextShared: shm_vaddr_base = %08"NACL_PRIxPTR"\n",
          shm_vaddr_base);
  shm_vaddr_base = NaClRoundAllocPage(shm_vaddr_base);
  NaClLog(4,
          "NaClMakeDynamicTextShared: shm_vaddr_base = %08"NACL_PRIxPTR"\n",
          shm_vaddr_base);
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
    mmap_ret = (*shm->base.vtbl->Map)((struct NaClDesc *) shm,
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
    if (-1 == (*shm->base.vtbl->UnmapUnsafe)((struct NaClDesc *) shm,
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
    mmap_ret = (*shm->base.vtbl->Map)((struct NaClDesc *) shm,
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
