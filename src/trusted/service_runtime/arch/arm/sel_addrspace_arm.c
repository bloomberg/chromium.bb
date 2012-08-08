/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/arch/sel_ldr_arch.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"


/* NOTE: This routine is almost identical to the x86_32 version.
 */
NaClErrorCode NaClAllocateSpace(void **mem, size_t addrsp_size) {
  int result;
  void *tmp_mem = (void *) NACL_TRAMPOLINE_START;

  CHECK(NULL != mem);

  /*
   * On ARM, we cheat slightly: we add two pages to the requested
   * allocation!  This accomodates the guard region we require at the
   * top end of untrusted memory.
   */
  addrsp_size += NACL_ADDRSPACE_UPPER_GUARD_SIZE;

  NaClAddrSpaceBeforeAlloc(addrsp_size);

  /*
   * On 32 bit Linux, a 1 gigabyte block of address space may be reserved at
   * the zero-end of the address space during process creation, to address
   * sandbox layout requirements on ARM and performance issues on Intel ATOM.
   * Look for this prereserved block and if found, pass its address to the
   * page allocation function.
   */
  if (!NaClFindPrereservedSandboxMemory(mem, addrsp_size)) {
    /* On ARM, we should always have prereserved sandbox memory. */
    NaClLog(LOG_ERROR, "NaClAllocateSpace:"
            " Could not find correct amount of prereserved memory"
            " (looked for 0x%016"NACL_PRIxS" bytes).\n",
            addrsp_size);
    return LOAD_NO_MEMORY;
  }
  /*
   * When creating a zero-based sandbox, we do not allocate the first 64K of
   * pages beneath the trampolines, because -- on Linux at least -- we cannot.
   * Instead, we allocate starting at the trampolines, and then coerce the
   * "mem" out parameter.
   */
  CHECK(*mem == NULL);
  addrsp_size -= NACL_TRAMPOLINE_START;
  result = NaCl_page_alloc_at_addr(&tmp_mem, addrsp_size);

  if (0 != result) {
    NaClLog(2,
            "NaClAllocateSpace: NaCl_page_alloc_at_addr 0x%08"NACL_PRIxPTR
            " failed\n",
            (uintptr_t) tmp_mem);
    return LOAD_NO_MEMORY;
  }
  NaClLog(4, "NaClAllocateSpace: %"NACL_PRIxPTR", %"NACL_PRIxS"\n",
          (uintptr_t) *mem,
          addrsp_size);

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
   * We need to create a two-page guard region at the base of
   * trusted memory, for write sandboxing.
   */
  NaClLog(4, "NaClMprotectGuards: %"NACL_PRIxPTR", %"NACL_PRIxS"\n",
          (uintptr_t) guard_base,
          (size_t) NACL_ADDRSPACE_UPPER_GUARD_SIZE);
  if ((err = NaCl_mprotect(guard_base,
                           NACL_ADDRSPACE_UPPER_GUARD_SIZE,
                           PROT_NONE)) != 0) {
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
