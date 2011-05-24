/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Service Runtime memory allocation code
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/machine/_types.h"


#define MSGWIDTH  "25"


void NaCl_page_free(void     *p,
                    size_t   size) {
  if (p == 0 || size == 0)
    return;
  if (munmap(p, size) != 0) {
    NaClLog(LOG_FATAL, "NaCl_page_free: munmap() failed");
  }
}


/*
 * Note that NaCl_page_alloc does not allocate pages that satisify
 * NaClIsAllocPageMultiple.  On linux/osx, the system does not impose
 * any such restrictions, and we only need to enforce the restriction
 * on NaCl app code to ensure that the app code is portable across all
 * host OSes.
 */

int NaCl_page_alloc_intern(void   **p,
                           size_t size) {
  void *addr;
  int map_flags = MAP_PRIVATE | MAP_ANONYMOUS;

#if NACL_LINUX
  /*
   * Indicate to the kernel that we just want these pages allocated, not
   * committed.  This is important for systems with relatively little RAM+swap.
   * See bug 251.
   */
  map_flags |= MAP_NORESERVE;
#elif NACL_OSX
  /*
   * TODO(cbiffle): Contrary to its name, this file is used by Mac OS X as well
   * as Linux.  An equivalent fix may require this to stop, since we might have
   * to drop to the xnu layer and use vm_allocate.
   *
   * Currently this code is not guaranteed to work for non-x86-32 Mac OS X.
   */
#else
# error This file should be included only by Linux and (surprisingly) OS X.
#endif

  if (NULL != *p) {
    map_flags |= MAP_FIXED;
  }
  NaClLog(4,
          "sel_memory: NaCl_page_alloc_intern:"
          " mmap(%p, %"NACL_PRIxS", %#x, %#x, %d, %"NACL_PRIdNACL_OFF64")\n",
          *p, size, PROT_NONE, map_flags, -1,
          (nacl_abi_off64_t) 0);
  addr = mmap(*p, size, PROT_NONE, map_flags, -1, (off_t) 0);
  if (MAP_FAILED == addr) {
    addr = NULL;
  }
  if (NULL != addr) {
    *p = addr;
  }
  return (NULL == addr) ? -ENOMEM : 0;
}

int NaCl_page_alloc(void   **p,
                    size_t size) {
  void *addr = NULL;
  int rv;

  if (0 == (rv = NaCl_page_alloc_intern(&addr, size))) {
    *p = addr;
  }

  return rv;
}

int NaCl_page_alloc_at_addr(void   **p,
                            size_t size) {
  return NaCl_page_alloc_intern(p, size);
}

/*
* This is critical to make the text region non-writable, and the data
* region read/write but no exec.  Of course, some kernels do not
* respect the lack of PROT_EXEC.
*/
int NaCl_mprotect(void          *addr,
                  size_t        len,
                  int           prot) {
  int  ret = mprotect(addr, len, prot);

  return ret == -1 ? -errno : ret;
}


int NaCl_madvise(void           *start,
                 size_t         length,
                 int            advice) {
  int ret = madvise(start, length, advice);

  /*
   * MADV_DONTNEED and MADV_NORMAL are needed
   */
  return ret == -1 ? -errno : ret;
}


void *NaClAllocatePow2AlignedMemory(size_t mem_sz, size_t log_alignment) {
  uintptr_t pow2align;
  size_t    request_sz;
  void      *mem_ptr;
  uintptr_t orig_addr;
  uintptr_t rounded_addr;
  size_t    extra;

  pow2align = ((uintptr_t) 1) << log_alignment;

  request_sz = mem_sz + pow2align;

  NaClLog(4,
          "%"MSGWIDTH"s %016"NACL_PRIxS"\n",
          " Ask:",
          request_sz);

  mem_ptr = mmap((void *) 0,
           request_sz,
           PROT_NONE,
           MAP_ANONYMOUS | MAP_NORESERVE | MAP_PRIVATE,
           -1,
           (off_t) 0);
  if (MAP_FAILED == mem_ptr) {
    return NULL;
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
