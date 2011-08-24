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
#define POST_ADDR_SPACE_GUARD_SIZE  (2 * NACL_PAGESIZE)

/* NOTE: This routine is almost identical to the x86_32 version.
 */
NaClErrorCode NaClAllocateSpace(void **mem, size_t addrsp_size) {
#if NACL_LINUX
  const void *ONE_MEGABYTE = (void *)(1024*1024);
#endif
  int result;

  CHECK(NULL != mem);

  addrsp_size += POST_ADDR_SPACE_GUARD_SIZE;
#if NACL_LINUX
  /*
   * When creating a zero-based sandbox, we do not allocate the first 64K of
   * pages beneath the trampolines, because -- on Linux at least -- we cannot.
   * Instead, we allocate starting at the trampolines, and then coerce the
   * "mem" out parameter.
   */
  addrsp_size -= NACL_TRAMPOLINE_START;
  /*
   * On 32 bit Linux, a 1 gigabyte block of address space may be reserved at
   * the zero-end of the address space during process creation, to address
   * sandbox layout requirements on ARM and performance issues on Intel ATOM.
   * Look for this pre-reserved block and if found, pass its address to the
   * page allocation function.
   */
  if (NaCl_find_prereserved_sandbox_memory(mem, addrsp_size)) {
    /* Sanity check zero sandbox base address.
     * It should be within a few pages above the 64KB boundary. See
     * chrome/nacl/nacl_helper_bootstrap.c in the Chromium repository
     * for more details.
     */
    if (0 == *mem || ONE_MEGABYTE < *mem) {
      NaClLog(LOG_ERROR, "NaClAllocateSpace:"
                         "  Can't handle sandbox at high address"
                         " 0x%08"NACL_PRIxPTR"\n",
              (uintptr_t)*mem);
      return LOAD_NO_MEMORY;
    }
  } else {
    /* Zero-based sandbox not pre-reserved. Try anyways.
     * TODO(bradchen): delete once new code is working.
     */
    *mem = (void *) NACL_TRAMPOLINE_START;
  }
  result = NaCl_page_alloc_at_addr(mem, addrsp_size);
  *mem = 0;
#else
# error "I only know how to allocate memory for ARM on Linux."
#endif
  if (0 != result) {
    NaClLog(2,
            "NaClAllocateSpace: NaCl_page_alloc_at_addr 0x%08"NACL_PRIxPTR
            " failed\n",
            (uintptr_t) *mem);
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
   * In ARM implementation kernel does not allow us to mmap address space at
   * address 0x0, so we mmap it at the start of a trampoline region.
   * Therefore, there is not need to mprotect at the start_addr.
   *
   * However, we do create a vmmap entry to describe it.
   */
  NaClLog(3,
          ("NULL detection region start 0x%08"NACL_PRIxPTR", "
           "size 0x%08x, end 0x%08"NACL_PRIxPTR"\n"),
          0, NACL_SYSCALL_START_ADDR,
          NACL_SYSCALL_START_ADDR);

  if (!NaClVmmapAdd(&nap->mem_map,
                    nap->mem_start >> NACL_PAGESHIFT,
                    NACL_SYSCALL_START_ADDR >> NACL_PAGESHIFT,
                    PROT_NONE,
                    (struct NaClMemObj *) NULL)) {
    NaClLog(LOG_ERROR, ("NaClMemoryProtection: NaClVmmapAdd failed"
                        " (NULL pointer guard page)\n"));
    return LOAD_MPROTECT_FAIL;
  }

  /*
   * We need to create a two-page guard region at the base of
   * trusted memory, for write sandboxing.
   */
  NaClLog(4, "NaClMprotectGuards: %"NACL_PRIxPTR", %"NACL_PRIxS"\n",
          (uintptr_t) guard_base,
          (size_t) POST_ADDR_SPACE_GUARD_SIZE);
  if ((err = NaCl_mprotect(guard_base,
                           POST_ADDR_SPACE_GUARD_SIZE,
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
