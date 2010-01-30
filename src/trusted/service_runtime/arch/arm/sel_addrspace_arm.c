/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"


/*
 * On ARM, we cheat slightly: we add two pages to the requested allocation!
 * This accomodates the guard region we require at the top end of untrusted
 * memory.
 */
int NaClAllocateSpace(void **mem, size_t size) {
  CHECK(mem);

  size += 2 * NACL_PAGESIZE;

  *mem = (void *) NACL_TRAMPOLINE_START;
  if (NaCl_page_alloc_at_addr(mem, size) != 0) {
    NaClLog(2,
        "NaClAlloccaterSpace: NaCl_page_alloc_at_addr 0x%08"PRIxPTR" failed\n",
        (uintptr_t) *mem);
    return LOAD_NO_MEMORY;
  }

  /*
   * makes sel_ldr think that the module's address space is at 0x0, this where
   * it should be
   */
  *mem = 0x0;

  return LOAD_OK;
}

/*
 * In the ARM sandboxing scheme we put the NaCl module at low virtual
 * address -- and thus there can only ever be one running NaCl app per
 * sel_ldr process -- to simplify the address masking etc needed for
 * the sandboxing.  All memory below ((uintptr_t) 1) << nap->addr_bits
 * is accessible to the NaCl app, and modulo page protection,
 * potentially writable.  Page protection is, of course, used to
 * prevent writing to text and rodata, including the trusted
 * trampoline code thunks.  To simplify write sandboxing, some
 * additional guard pages outside of the address space is needed, so
 * that we don't have to additionally check the effective address
 * obtained by register-relative addressing modes.
 */
NaClErrorCode NaClMprotectGuards(struct NaClApp *nap) {
  int err;
  void *guard_base = (void *) (((uintptr_t) 1) << nap->addr_bits);
  /*
   * In ARM implementation kernel does not allow us to mmap address space at
   * address 0x0, so we mmap it at the start of a trampoline region.
   * Therefore, there is not need to mprotect at the start_addr.
   */

  /*
   * However, we need to create a two-page guard region at the base of
   * trusted memory, for write sandboxing.
   */
  if ((err = NaCl_mprotect(guard_base, 2 * NACL_PAGESIZE, PROT_NONE)) != 0) {
    NaClLog(LOG_ERROR, ("NaClMemoryProtection: failed to protect lower guard "
                        "on trusted memory space (error %d)\n"),
            err);
    return LOAD_MPROTECT_FAIL;
  }

  /*
   * NB: the pages just mapped are OUTSIDE of the address space of the
   * NaCl module.  We should not track them in the Vmmap structure,
   * since that's to track addressable memory for mapping and unmapping.
   *
   * This means that because these pages are implicit and not tracked,
   * we should have a hook to tear down these pages as part of the
   * NaClApp dtor.
   */
  return LOAD_OK;
}

void NaClTeardownMprotectGuards(struct NaClApp *nap) {
  void *guard_base = (void *) (((uintptr_t) 1) << nap->addr_bits);

  NaCl_page_free(guard_base, 2 * NACL_PAGESIZE);
}
