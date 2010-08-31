// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The allocator is very simplistic. It requests memory pages directly from
// the system. Each page starts with a header describing the allocation. This
// makes sure that we can return the memory to the system when it is
// deallocated.
// For allocations that are smaller than a single page, we try to squeeze
// multiple of them into the same page.
// We expect to use this allocator for a moderate number of small allocations.
// In most cases, it will only need to ever make a single request to the
// operating system for the lifetime of the STL container object.
// We don't worry about memory fragmentation as the allocator is expected to
// be short-lived.

#include <stdint.h>
#include <sys/mman.h>

#include "allocator.h"
#include "linux_syscall_support.h"

namespace playground {

class SysCalls {
 public:
  #define SYS_CPLUSPLUS
  #define SYS_ERRNO     my_errno
  #define SYS_INLINE    inline
  #define SYS_PREFIX    -1
  #undef  SYS_LINUX_SYSCALL_SUPPORT_H
  #include "linux_syscall_support.h"
  SysCalls() : my_errno(0) { }
  int my_errno;
};
#ifdef __NR_mmap2
  #define      MMAP      mmap2
  #define __NR_MMAP __NR_mmap2
#else
  #define      MMAP      mmap
  #define __NR_MMAP __NR_mmap
#endif

// We only ever keep track of the very last partial page that was used for
// allocations. This approach simplifies the code a lot. It can theoretically
// lead to more memory fragmentation, but for our use case that is unlikely
// to happen.
struct Header {
  // The total amount of memory allocated for this chunk of memory. Typically,
  // this would be a single page.
  size_t total_len;

  // "used" keeps track of the number of bytes currently allocated in this
  // page. Note that as elements are freed from this page, "used" is updated
  // allowing us to track when the page is free. However, these holes in the
  // page are never re-used, so "tail" is the only way to find out how much
  // free space remains and when we need to request another chunk of memory
  // from the system.
  size_t used;
  void   *tail;
};
static Header* last_alloc;

void* SystemAllocatorHelper::sys_allocate(size_t size) {
  // Number of bytes that need to be allocated
  if (size + 3 < size) {
    return NULL;
  }
  size_t len = (size + 3) & ~3;

  if (last_alloc) {
    // Remaining space in the last chunk of memory allocated from system
    size_t remainder = last_alloc->total_len -
        (reinterpret_cast<char *>(last_alloc->tail) -
         reinterpret_cast<char *>(last_alloc));

    if (remainder >= len) {
      void* ret = last_alloc->tail;
      last_alloc->tail = reinterpret_cast<char *>(last_alloc->tail) + len;
      last_alloc->used += len;
      return ret;
    }
  }

  SysCalls sys;
  if (sizeof(Header) + len + 4095 < len) {
    return NULL;
  }
  size_t total_len = (sizeof(Header) + len + 4095) & ~4095;
  Header* mem = reinterpret_cast<Header *>(
      sys.MMAP(NULL, total_len, PROT_READ|PROT_WRITE,
               MAP_PRIVATE|MAP_ANONYMOUS, -1, 0));
  if (mem == MAP_FAILED) {
    return NULL;
  }

  // If we were only asked to allocate a single page, then we will use any
  // remaining space for other small allocations.
  if (total_len - sizeof(Header) - len >= 4) {
    last_alloc = mem;
  }
  mem->total_len = total_len;
  mem->used = len;
  char* ret = reinterpret_cast<char *>(mem) + sizeof(Header);
  mem->tail = ret + len;

  return ret;
}

void SystemAllocatorHelper::sys_deallocate(void* p, size_t size) {
  // Number of bytes in this allocation
  if (size + 3 < size) {
    return;
  }
  size_t len = (size + 3) & ~3;

  // All allocations (small and large) have starting addresses in the
  // first page that was allocated from the system. This page starts with
  // a header that keeps track of how many bytes are currently used. The
  // header can be found by truncating the last few bits of the address.
  Header* header = reinterpret_cast<Header *>(
      reinterpret_cast<uintptr_t>(p) & ~4095);
  header->used -= len;

  // After the last allocation has been freed, return the page(s) to the
  // system
  if (!header->used) {
    SysCalls sys;
    sys.munmap(header, header->total_len);
    if (last_alloc == header) {
      last_alloc = NULL;
    }
  }
}

}  // namespace
