/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#if NACL_LINUX
#include <errno.h>
#include <sys/mman.h>
#endif

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


NaClErrorCode NaClAllocateSpace(void **mem, size_t addrsp_size) {
#if NACL_LINUX
  const void *ONE_MEGABYTE = (void *)(1024*1024);
#endif
  int result;

  CHECK(NULL != mem);

  NaClAddrSpaceBeforeAlloc(addrsp_size);

#if NACL_LINUX
  /*
   * On 32 bit Linux, a 1 gigabyte block of address space may be reserved at
   * the zero-end of the address space during process creation, to address
   * sandbox layout requirements on ARM and performance issues on Intel ATOM.
   * Look for this pre-reserved block and if found, pass its address to the
   * page allocation function.
   */
  if (NaClFindPrereservedSandboxMemory(mem, addrsp_size)) {
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

    /*
     * When creating a zero-based sandbox, we do not allocate the first 64K of
     * pages beneath the trampolines, because -- on Linux at least -- we cannot.
     * Instead, we allocate starting at the trampolines, and then coerce the
     * "mem" out parameter.
     */
    addrsp_size -= NACL_TRAMPOLINE_START;
    *mem = (void *) NACL_TRAMPOLINE_START;
    result = NaCl_page_alloc_at_addr(mem, addrsp_size);
    *mem = 0;
  } else {
    /* Zero-based sandbox not pre-reserved. Attempt to allocate anyway. */
    result = NaCl_page_alloc(mem, addrsp_size);
  }
#elif NACL_WINDOWS
  /*
   * On 32 bit Windows, a 1 gigabyte block of address space is reserved before
   * starting up this process to make sure we can create the sandbox. Look for
   * this pre-reserved block and if found, pass its address to the page
   * allocation function.
   */
  if (0 == NaClFindPrereservedSandboxMemory(mem, addrsp_size)) {
    result = NaCl_page_alloc_at_addr(mem, addrsp_size);
  } else {
    result = NaCl_page_alloc(mem, addrsp_size);
  }
#else
  result = NaCl_page_alloc(mem, addrsp_size);
#endif

  if (0 != result) {
    NaClLog(2,
        "NaClAllocateSpace: NaCl_page_alloc 0x%08"NACL_PRIxPTR
        " failed\n",
        (uintptr_t) *mem);
    return LOAD_NO_MEMORY;
  }
  NaClLog(4, "NaClAllocateSpace: %"NACL_PRIxPTR", %"NACL_PRIxS"\n",
          (uintptr_t) *mem,
          addrsp_size);

  return LOAD_OK;
}


NaClErrorCode NaClMprotectGuards(struct NaClApp *nap) {
  uintptr_t start_addr;
  int       err;
#if NACL_LINUX
  uintptr_t page_addr;
  void     *mmap_rval;
#endif

  start_addr = nap->mem_start;

  NaClLog(3,
          ("NULL detection region start 0x%08"NACL_PRIxPTR", "
           " size 0x%08x, end 0x%08"NACL_PRIxPTR"\n"),
          start_addr, NACL_SYSCALL_START_ADDR,
          start_addr + NACL_SYSCALL_START_ADDR);
  if (0 == start_addr) {
#if !NACL_LINUX
    NaClLog(LOG_FATAL, ("NaClMprotectGuards: zero-based sandbox is"
                        " supported on Linux only.\n"));
#else
    /*
     * Attempt to protect low memory from zero to NACL_SYSCALL_START_ADDR,
     * the base of the region allocated in Chrome by nacl_helper.
     *
     * It is normal for mmap() calls to fail with EPERM if the indicated
     * page is less than vm.mmap_min_addr (see /proc/sys/vm/mmap_min_addr),
     * or with EACCES under SELinux if less than CONFIG_LSM_MMAP_MIN_ADDR
     * (64k).  Hence, we adaptively move the bottom of the region up a page
     * at a time until we succeed in getting a reservation.
     */
    for (page_addr = 0;
         page_addr < NACL_SYSCALL_START_ADDR;
         page_addr += NACL_PAGESIZE) {
      mmap_rval = mmap((void *) page_addr,
                       NACL_SYSCALL_START_ADDR - page_addr,
                       PROT_NONE,
                       MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS | MAP_NORESERVE,
                       -1, 0);
      if (page_addr == (uintptr_t)mmap_rval) {
        /* Success; the pages are now protected. */
        break;
      } else if (MAP_FAILED == mmap_rval &&
                 (EPERM == errno || EACCES == errno)) {
        /* Normal; this is an invalid page for this process and
         * doesn't need to be protected. Continue with next page.
         */
      } else {
        NaClLog(LOG_ERROR, ("NaClMemoryProtection: "
                            " mmap(0x%08"NACL_PRIxPTR") failed, "
                            " error %d (NULL pointer guard page)\n"),
                page_addr, errno);
        return LOAD_MPROTECT_FAIL;
      }
    }
#endif
  } else {
    err = NaCl_mprotect((void *) start_addr,
                        NACL_SYSCALL_START_ADDR,
                        PROT_NONE);
    if (err != 0) {
      NaClLog(LOG_ERROR, ("NaClMemoryProtection:"
                          " NaCl_mprotect(0x%08"NACL_PRIxPTR","
                          " 0x%08x, 0x%x) failed,"
                          " error %d (NULL pointer guard page)\n"),
              start_addr, NACL_SYSCALL_START_ADDR, PROT_NONE,
              err);
      return LOAD_MPROTECT_FAIL;
    }
  }
  if (!NaClVmmapAdd(&nap->mem_map,
                    (start_addr - nap->mem_start) >> NACL_PAGESHIFT,
                    NACL_SYSCALL_START_ADDR >> NACL_PAGESHIFT,
                    PROT_NONE,
                    (struct NaClMemObj *) NULL)) {
    NaClLog(LOG_ERROR, ("NaClMemoryProtection: NaClVmmapAdd failed"
                        " (NULL pointer guard page)\n"));
    return LOAD_MPROTECT_FAIL;
  }

  return LOAD_OK;
}
