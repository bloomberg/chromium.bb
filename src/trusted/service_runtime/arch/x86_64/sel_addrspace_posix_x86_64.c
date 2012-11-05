/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <sys/mman.h>
#if NACL_LINUX
/*
 * For getrlimit.
 */
# include <sys/resource.h>
#endif

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_global_secure_random.h"
#include "native_client/src/trusted/service_runtime/arch/sel_ldr_arch.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"


#define FOURGIG     (((size_t) 1) << 32)
#define ALIGN_BITS  32
#define MSGWIDTH    "25"


/*
 * NaClAllocatePow2AlignedMemory is for allocating a large amount of
 * memory of mem_sz bytes that must be address aligned, so that
 * log_alignment low-order address bits must be zero.
 *
 * This function attempts to randomize the high order bits of the
 * address by supplying a non-zero address to mmap as a hint (without
 * MAP_FIXED).  NaClGlobalSecureRngUint32 must be usable, i.e., the
 * NaClGlobalSecureRngInit module initializer (typically via
 * NaClPlatformInit) must have been called.
 *
 * Returns the aligned region on success, or NULL on failure.
 */
static void *NaClAllocatePow2AlignedMemory(size_t mem_sz,
                                           size_t log_alignment) {
  /*
   * The x86-64 (currently) permits only a 48 bit virtual address
   * space (and requires that the upper 64-48=16 bits is sign extended
   * from bit 47).  Additionally, the linux kernel disallows negative
   * addresses for user-space code.  So instead of 48, we get a
   * maximum of 47 usable bits of virtual address.
   */
  int const kNumUserAddressBits = 47;
  int const kNumTries = 4;
  int       attempt;
  uintptr_t addr_high_bits_mask;
  uintptr_t addr_high_bits_hint;
  uintptr_t pow2align;
  size_t    request_sz;
  void      *mem_ptr;
  uintptr_t orig_addr;
  uintptr_t rounded_addr;
  size_t    extra;

  pow2align = ((uintptr_t) 1) << log_alignment;
  addr_high_bits_mask = ~(pow2align - 1);
  addr_high_bits_mask &= ((uintptr_t) 1 << kNumUserAddressBits) - 1;

  request_sz = mem_sz + pow2align;

  NaClLog(4,
          "%"MSGWIDTH"s %016"NACL_PRIxS"\n",
          " Ask:",
          request_sz);

  attempt = 0;
  for (;;) {
    addr_high_bits_hint = ((((uintptr_t) NaClGlobalSecureRngUint32()) << 32) |
                           (uintptr_t) NaClGlobalSecureRngUint32());
    addr_high_bits_hint &= addr_high_bits_mask;

    mem_ptr = mmap((void *) addr_high_bits_hint,
                   request_sz,
                   PROT_NONE,
                   MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE,
                   -1,
                   (off_t) 0);
    NaClLog(4,
            ("NaClAllocatePow2AlignedMemory: attempt %d, hint %"NACL_PRIxPTR
             ", got address %"NACL_PRIxPTR"\n"),
            attempt, addr_high_bits_hint, (uintptr_t) mem_ptr);
    if (MAP_FAILED == mem_ptr) {
      return NULL;
    }
    /*
     * mmap will use a system-dependent algorithm to find a starting
     * address if the hint location cannot work, e.g., if there is
     * already memory mapped there.  We do not trust that algorithm to
     * provide any randomness in the high-order bits, so we only
     * accept allocations that match the requested high-order bits
     * exactly.  If the algorithm is to scan the address space
     * starting near the hint address, then it would be acceptable; if
     * it is to use a default algorithm that is independent of the
     * supplied hint, then it would not be.
     */
    if ((addr_high_bits_mask & ((uintptr_t) mem_ptr)) ==
        addr_high_bits_hint) {
      /* success */
      break;
    }
    if (++attempt == kNumTries) {
      /*
       * We give up trying to get the random high-order bits and just
       * use the last mapping that the system returned to us.
       */
      break;
    }
    /*
     * Remove undesirable mapping location before trying again.
     */
    if (-1 == munmap(mem_ptr, request_sz)) {
      NaClLog(LOG_FATAL,
              "NaClAllocatePow2AlignedMemory: could not unmap non-random"
              " memory\n");
    }
  }
  orig_addr = (uintptr_t) mem_ptr;

  NaClLog(4,
          "%"MSGWIDTH"s %016"NACL_PRIxPTR"\n",
          "orig memory at",
          orig_addr);

  rounded_addr = (orig_addr + (pow2align - 1)) & ~(pow2align - 1);
  extra = rounded_addr - orig_addr;

  if (0 != extra) {
    NaClLog(4,
            "%"MSGWIDTH"s %016"NACL_PRIxPTR", %016"NACL_PRIxS"\n",
            "Freeing front:",
            orig_addr,
            extra);
    if (-1 == munmap((void *) orig_addr, extra)) {
      perror("munmap (front)");
      NaClLog(LOG_FATAL,
              "NaClAllocatePow2AlignedMemory: munmap front failed\n");
    }
  }

  extra = pow2align - extra;
  if (0 != extra) {
    NaClLog(4,
            "%"MSGWIDTH"s %016"NACL_PRIxPTR", %016"NACL_PRIxS"\n",
            "Freeing tail:",
            rounded_addr + mem_sz,
            extra);
    if (-1 == munmap((void *) (rounded_addr + mem_sz),
         extra)) {
      perror("munmap (end)");
      NaClLog(LOG_FATAL,
              "NaClAllocatePow2AlignedMemory: munmap tail failed\n");
    }
  }
  NaClLog(4,
          "%"MSGWIDTH"s %016"NACL_PRIxPTR"\n",
          "Aligned memory:",
          rounded_addr);

  /*
   * we could also mmap again at rounded_addr w/o MAP_NORESERVE etc to
   * ensure that we have the memory, but that's better done in another
   * utility function.  the semantics here is no paging space
   * reserved, as in Windows MEM_RESERVE without MEM_COMMIT.
   */

  return (void *) rounded_addr;
}

NaClErrorCode NaClAllocateSpace(void **mem, size_t addrsp_size) {
  /* 40G guard on each side */
  size_t        mem_sz = (NACL_ADDRSPACE_LOWER_GUARD_SIZE + FOURGIG +
                          NACL_ADDRSPACE_UPPER_GUARD_SIZE);
  size_t        log_align = ALIGN_BITS;
  void          *mem_ptr;
#if NACL_LINUX
  struct rlimit rlim;
#endif

  NaClLog(4, "NaClAllocateSpace(*, 0x%016"NACL_PRIxS" bytes).\n",
          addrsp_size);

  CHECK(addrsp_size == FOURGIG);

  if (NACL_X86_64_ZERO_BASED_SANDBOX) {
    mem_sz = 11 * FOURGIG;
    if (getenv("NACL_ENABLE_INSECURE_ZERO_BASED_SANDBOX") != NULL) {
      /*
       * For the zero-based 64-bit sandbox, we want to reserve 44GB of address
       * space: 4GB for the program plus 40GB of guard pages.  Due to a binutils
       * bug (see http://sourceware.org/bugzilla/show_bug.cgi?id=13400), the
       * amount of address space that the linker can pre-reserve is capped
       * at 4GB. For proper reservation, GNU ld version 2.22 or higher
       * needs to be used.
       *
       * Without the bug fix, trying to reserve 44GB will result in
       * pre-reserving the entire capped space of 4GB.  This tricks the run-time
       * into thinking that we can mmap up to 44GB.  This is unsafe as it can
       * overwrite the run-time program itself and/or other programs.
       *
       * For now, we allow a 4GB address space as a proof-of-concept insecure
       * sandboxing model.
       *
       * TODO(arbenson): remove this if block once the binutils bug is fixed
       */
      mem_sz = FOURGIG;
    }

    NaClAddrSpaceBeforeAlloc(mem_sz);
    if (NaClFindPrereservedSandboxMemory(mem, mem_sz)) {
      int result;
      void *tmp_mem = (void *) NACL_TRAMPOLINE_START;
      CHECK(*mem == 0);
      mem_sz -= NACL_TRAMPOLINE_START;
      result = NaCl_page_alloc_at_addr(&tmp_mem, mem_sz);
      if (0 != result) {
        NaClLog(2,
                "NaClAllocateSpace: NaCl_page_alloc 0x%08"NACL_PRIxPTR
                " failed\n",
                (uintptr_t) *mem);
        return LOAD_NO_MEMORY;
      }
      NaClLog(4, "NaClAllocateSpace: %"NACL_PRIxPTR", %"NACL_PRIxS"\n",
              (uintptr_t) *mem,
              mem_sz);
      return LOAD_OK;
    }
    NaClLog(LOG_ERROR, "Failed to find prereserved memory\n");
    return LOAD_NO_MEMORY;
  }

  NaClAddrSpaceBeforeAlloc(mem_sz);

  errno = 0;
  mem_ptr = NaClAllocatePow2AlignedMemory(mem_sz, log_align);
  if (NULL == mem_ptr) {
    if (0 != errno) {
      perror("NaClAllocatePow2AlignedMemory");
    }
    NaClLog(LOG_WARNING, "Memory allocation failed\n");
#if NACL_LINUX
    /*
     * Check with getrlimit whether RLIMIT_AS was likely to be the
     * problem with an allocation failure.  If so, generate a log
     * message.  Since this is a debugging aid and we don't know about
     * the memory requirement of the code that is embedding native
     * client, there is some slop.
     */
    if (0 != getrlimit(RLIMIT_AS, &rlim)) {
      perror("NaClAllocatePow2AlignedMemory::getrlimit");
    } else {
      if (rlim.rlim_cur < mem_sz) {
        /*
         * Developer hint/warning; this will show up in the crash log
         * and must be brief.
         */
        NaClLog(LOG_INFO,
                "Please run \"ulimit -v unlimited\" (bash)"
                " or \"limit vmemoryuse unlimited\" (tcsh)\n");
        NaClLog(LOG_INFO,
                "and restart the app.  NaCl requires at least %"NACL_PRIdS""
                " kilobytes of virtual\n",
                mem_sz / 1024);
        NaClLog(LOG_INFO,
                "address space. NB: Raising the hard limit requires"
                " root access.\n");
      }
    }
#elif NACL_OSX
    /*
     * In OSX, RLIMIT_AS and RLIMIT_RSS have the same value; i.e., OSX
     * conflates the notion of virtual address space used with the
     * resident set size.  In particular, the way NaCl uses virtual
     * address space is to allocate guard pages so that certain
     * addressing modes will not need to be explicitly masked; the
     * guard pages are allocated but inaccessible, never faulted so
     * not even zero-filled on demand, so they should not count
     * against the resident set -- which is supposed to be only the
     * frequently accessed pages in the first place.
     */
#endif

    return LOAD_NO_MEMORY;
  }
  /*
   * The module lives in the middle FOURGIG of the allocated region --
   * we skip over an initial 40G guard.
   */
  *mem = (void *) (((char *) mem_ptr) + NACL_ADDRSPACE_LOWER_GUARD_SIZE);
  NaClLog(4,
          "NaClAllocateSpace: addr space at 0x%016"NACL_PRIxPTR"\n",
          (uintptr_t) *mem);

  return LOAD_OK;
}
