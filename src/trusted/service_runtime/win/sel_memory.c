/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Service Runtime memory allocation code
 */
#include "native_client/src/include/portability.h"
#include "native_client/src/include/win/mman.h"

#include <errno.h>
#include <windows.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/win/xlate_system_error.h"

#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"

#define MSGWIDTH  "25"

/*
 * NaCl_page_free: free pages allocated with NaCl_page_alloc.
 * Must start at allocation granularity (NACL_MAP_PAGESIZE) and
 * number of bytes must be a multiple of allocation granularity.
 */
void  NaCl_page_free(void     *p,
                     size_t   num_bytes) {
  void  *end_addr;

  end_addr = (void *) (((char *) p) + num_bytes);
  while (p < end_addr) {
    if (!VirtualFree(p, 0, MEM_RELEASE)) {
      DWORD err = GetLastError();
      NaClLog(LOG_FATAL,
              "NaCl_page_free: VirtualFree(0x%016"NACL_PRIxPTR
              ", 0, MEM_RELEASE) failed "
              "with error 0x%X\n",
              (uintptr_t) p, err);
    }
    p = (void *) (((char *) p) + NACL_MAP_PAGESIZE);
  }
}


int   NaCl_page_alloc(void    **p,
                      size_t  num_bytes) {
  SYSTEM_INFO sys_info;

  int         attempt_count;

  void        *addr;
  void        *end_addr;
  void        *chunk;
  void        *unroll;

  /*
   * We have to allocate every 64KB -- the windows allocation
   * granularity -- because VirtualFree will only accept an address
   * that was returned by a call to VirtualAlloc.  NB: memory pages
   * put into the address space via MapViewOfFile{,Ex} must be
   * released by UnmapViewOfFile.  Thus, in order for us to open up a
   * hole in the NaCl application's address space to map in a file, we
   * must allocate the entire address space in 64KB chunks, so we can
   * later pick an arbitrary range of addresses (in 64KB chunks) to
   * free up and map in a file later.
   *
   * First, we verify via GetSystemInfo that the allocation
   * granularity matches NACL_MAP_PAGESIZE.
   *
   * Next, we VirtualAlloc the entire chunk desired.  This essentially
   * asks the kernel where there is space in the virtual address
   * space.  Then, we free this back, and repeat the allocation
   * starting at the returned address, but in 64KB chunks.  If any of
   * these smaller allocations fail, we roll back and try again.
   */

  NaClLog(3, "NaCl_page_alloc(*, 0x%"NACL_PRIxS")\n", num_bytes);
  GetSystemInfo(&sys_info);
  if (NACL_PAGESIZE != sys_info.dwPageSize) {
    NaClLog(2, "page size is 0x%x; expected 0x%x\n",
            sys_info.dwPageSize,
            NACL_PAGESIZE);
  }
  if (NACL_MAP_PAGESIZE != sys_info.dwAllocationGranularity) {
    NaClLog(0, "allocation granularity is 0x%x; expected 0x%x\n",
            sys_info.dwAllocationGranularity,
            NACL_MAP_PAGESIZE);
  }

  /*
   * Round allocation request up to next NACL_MAP_PAGESIZE.  This is
   * assumed to have taken place in NaCl_page_free.
   */
  num_bytes = NaClRoundAllocPage(num_bytes);

  for (attempt_count = 0;
       attempt_count < NACL_MEMORY_ALLOC_RETRY_MAX;
       ++attempt_count) {

    addr = VirtualAlloc(NULL, num_bytes, MEM_RESERVE, PAGE_NOACCESS);
    if (addr == NULL) {
      NaClLog(0,
              "NaCl_page_alloc: VirtualAlloc(*,0x%"NACL_PRIxS") failed\n",
              num_bytes);
      return -ENOMEM;
    }
    NaClLog(3,
            ("NaCl_page_alloc:"
             "  VirtualAlloc(*,0x%"NACL_PRIxS")"
             " succeeded, 0x%016"NACL_PRIxPTR","
             " releasing and re-allocating in 64K chunks....\n"),
            num_bytes, (uintptr_t) addr);
    (void) VirtualFree(addr, 0, MEM_RELEASE);
    /*
     * We now know [addr, addr + num_bytes) is available in our addr space.
     * Get that memory again, but in 64KB chunks.
     */
    end_addr = (void *) (((char *) addr) + num_bytes);
    for (chunk = addr;
         chunk < end_addr;
         chunk = (void *) (((char *) chunk) + NACL_MAP_PAGESIZE)) {
      if (NULL == VirtualAlloc(chunk,
                               NACL_MAP_PAGESIZE,
                               MEM_COMMIT | MEM_RESERVE,
                               PAGE_NOACCESS)) {
        NaClLog(0, ("NaCl_page_alloc: re-allocation failed at "
                    "0x%016"NACL_PRIxPTR","
                    " error %d\n"),
                (uintptr_t) chunk, GetLastError());
        for (unroll = addr;
             unroll < chunk;
             unroll = (void *) (((char *) unroll) + NACL_MAP_PAGESIZE)) {
          (void) VirtualFree(unroll, 0, MEM_RELEASE);
        }
        goto retry;
        /* double break */
      }
    }
    NaClLog(2,
            ("NaCl_page_alloc: *p = 0x%016"NACL_PRIxPTR","
             " returning success.\n"),
            (uintptr_t) addr);
    *p = addr;
    return 0;
 retry:
    ;
  }

  return -ENOMEM;
}


uintptr_t NaClProtectChunkSize(uintptr_t start,
                               uintptr_t end) {
  uintptr_t chunk_end;

  chunk_end = NaClRoundAllocPage(start + 1);
  if (chunk_end > end) {
    chunk_end = end;
  }
  return chunk_end - start;
}


/*
 * This is critical to make the text region non-writable, and the data
 * region read/write but no exec.  Of course, some kernels do not
 * respect the lack of PROT_EXEC.
 */
int   NaCl_mprotect(void          *addr,
                    size_t        len,
                    int           prot) {
  uintptr_t start_addr;
  uintptr_t end_addr;
  uintptr_t cur_addr;
  uintptr_t cur_chunk_size;
  DWORD     newProtection, oldProtection;
  BOOL      res;
  int       xlated_res;

  NaClLog(2, "NaCl_mprotect(0x%016"NACL_PRIxPTR", 0x%"NACL_PRIxS", 0x%x)\n",
          (uintptr_t) addr, len, prot);

  if (len == 0) {
    return 0;
  }

  start_addr = (uintptr_t) addr;
  if (!NaClIsPageMultiple(start_addr)) {
    NaClLog(2, "NaCl_mprotect: start address not a multiple of page size\n");
    return -EINVAL;
  }
  if (!NaClIsPageMultiple(len)) {
    NaClLog(2, "NaCl_mprotect: length not a multiple of page size\n");
    return -EINVAL;
  }
  end_addr = start_addr + len;

  switch (prot) {
    case PROT_EXEC: {
      newProtection = PAGE_EXECUTE;
      break;
    }
    case PROT_EXEC|PROT_READ: {
      newProtection = PAGE_EXECUTE_READ;
      break;
    }
    case PROT_EXEC|PROT_READ|PROT_WRITE: {
      newProtection = PAGE_EXECUTE_READWRITE;
      break;
    }
    case PROT_READ: {
      newProtection = PAGE_READONLY;
      break;
    }
    case PROT_READ|PROT_WRITE: {
      newProtection = PAGE_READWRITE;
      break;
    }
    case PROT_NONE: {
      newProtection = PAGE_NOACCESS;
      break;
    }
    default: {
      NaClLog(2, "NaCl_mprotect: invalid protection mode\n");
      return -EINVAL;
    }
  }
  NaClLog(2, "NaCl_mprotect: newProtection = 0x%x\n", newProtection);
  /*
   * VirtualProtect region cannot span allocations: all addresses from
   * [lpAddress, lpAddress+dwSize) must be in one region of memory
   * returned from VirtualAlloc or VirtualAllocEx
   */
  for (cur_addr = start_addr,
           cur_chunk_size = NaClProtectChunkSize(cur_addr, end_addr);
       cur_addr < end_addr;
       cur_addr += cur_chunk_size,
           cur_chunk_size = NaClProtectChunkSize(cur_addr, end_addr)) {
    NaClLog(5,
            "NaCl_mprotect: VirtualProtect(0x%016"NACL_PRIxPTR","
            " 0x%"NACL_PRIxPTR", 0x%x, *)\n",
            cur_addr, cur_chunk_size, newProtection);

    if (newProtection != PAGE_NOACCESS) {
      res = VirtualProtect((void*) cur_addr,
                           cur_chunk_size,
                           newProtection,
                           &oldProtection);
      if (!res) {
        void *p;
        NaClLog(5, "NaCl_mprotect: ... failed; trying VirtualAlloc instead\n");
        p = VirtualAlloc((void*) cur_addr,
                         cur_chunk_size,
                         MEM_COMMIT,
                         newProtection);
        if (p != (void*) cur_addr) {
          NaClLog(2, "NaCl_mprotect: ... failed\n");
          return -NaClXlateSystemError(GetLastError());
        }
      }
    } else {
      /*
       * decommit the memory--this has the same effect as setting the protection
       * level to PAGE_NOACCESS, with the added benefit of not taking up any
       * swap space.
       */
      if (!VirtualFree((void *) cur_addr, cur_chunk_size, MEM_DECOMMIT)) {
        xlated_res = NaClXlateSystemError(GetLastError());
        NaClLog(2, "NaCl_mprotect: VirtualFree failed: 0x%x\n", xlated_res);
        return -xlated_res;
      }
    }
  }
  NaClLog(2, "NaCl_mprotect: done\n");
  return 0;
}


int   NaCl_madvise(void           *start,
                   size_t         length,
                   int            advice) {
  int       err;
  uintptr_t start_addr;
  uintptr_t end_addr;
  uintptr_t cur_addr;
  uintptr_t cur_chunk_size;

  /*
   * MADV_DONTNEED and MADV_NORMAL are needed
   */
  NaClLog(2, "NaCl_madvise(0x%016"NACL_PRIxPTR", 0x%"NACL_PRIxS", 0x%x)\n",
          (uintptr_t) start, length, advice);
  switch (advice) {
    case MADV_DONTNEED:
      start_addr = (uintptr_t) start;
      if (!NaClIsPageMultiple(start_addr)) {
        NaClLog(2,
                "NaCl_madvise: start address not a multiple of page size\n");
        return -EINVAL;
      }
      if (!NaClIsPageMultiple(length)) {
        NaClLog(2, "NaCl_madvise: length not a multiple of page size\n");
        return -EINVAL;
      }
      end_addr = start_addr + length;
      for (cur_addr = start_addr,
               cur_chunk_size = NaClProtectChunkSize(cur_addr, end_addr);
           cur_addr < end_addr;
           cur_addr += cur_chunk_size,
               cur_chunk_size = NaClProtectChunkSize(cur_addr, end_addr)) {
        NaClLog(4,
                ("NaCl_madvise: MADV_DONTNEED"
                 " -> VirtualAlloc(0x%016"NACL_PRIxPTR","
                 " 0x%"NACL_PRIxPTR", MEM_RESET, PAGE_NOACCESS)\n"),
                cur_addr, cur_chunk_size);

        /*
         * Decommit (but do not release) the page. This allows the kernel to
         * release backing store, but does not release the VA space. Should be
         * fairly close to the behavior we'd get from the Linux madvise()
         * function.
         */
        if (NULL == VirtualAlloc((void *) cur_addr,
                                 cur_chunk_size, MEM_RESET, PAGE_READONLY)) {
          err = NaClXlateSystemError(GetLastError());
          NaClLog(2, "NaCl_madvise: VirtualFree failed: 0x%x\n", err);
          return -err;
        }
      }
      break;
    case MADV_NORMAL:
      memset(start, 0, length);
      break;
    default:
      return -EINVAL;
  }
  NaClLog(2, "NaCl_madvise: done\n");
  return 0;
}


void *NaClAllocatePow2AlignedMemory(size_t mem_sz, size_t log_alignment) {
  uintptr_t pow2align;
  size_t    request_sz;
  uintptr_t mem_ptr;
  uintptr_t orig_addr;
  uintptr_t rounded_addr;
  size_t    extra;
  BOOL ok;

  pow2align = ((uintptr_t) 1) << log_alignment;

  request_sz = mem_sz + pow2align;

  NaClLog(LOG_INFO,
          "%"MSGWIDTH"s %016"NACL_PRIxS"\n",
          " Ask:",
          request_sz);

  mem_ptr = (uintptr_t) VirtualAlloc(NULL,
                                    request_sz,
                                    MEM_RESERVE,
                                    PAGE_READWRITE);
  if (0 == mem_ptr) {
    NaClLog(LOG_FATAL,
            "VirtualAlloc could not find space for %"NACL_PRIxS" bytes\n",
            request_sz);
  }

  orig_addr = (uintptr_t) mem_ptr;

  NaClLog(LOG_INFO,
          "%"MSGWIDTH"s %016"NACL_PRIxPTR"\n",
          "orig memory at",
          orig_addr);

  rounded_addr = (orig_addr + (pow2align - 1)) & ~(pow2align - 1);
  extra = rounded_addr - orig_addr;

  ok = VirtualFree((void *) orig_addr, 0, MEM_RELEASE);
  if (!ok) {
    NaClLog(LOG_FATAL,"VirtualFree failed for address %"NACL_PRIxPTR"\n",
            orig_addr);
  }

  for (mem_ptr = rounded_addr;
       mem_ptr < rounded_addr + mem_sz;
       mem_ptr += NACL_MAP_PAGESIZE) {
    void *p = VirtualAlloc((void *) mem_ptr,
                           NACL_MAP_PAGESIZE,
                           MEM_RESERVE,
                           PAGE_READWRITE);
    if ((uintptr_t) p != mem_ptr) {
      NaClLog(LOG_FATAL, "Failed to reserve page at %"NACL_PRIxPTR"\n", p);
    }
  }

  NaClLog(LOG_INFO,
          "%"MSGWIDTH"s %016"NACL_PRIxPTR"\n",
          "Aligned memory:",
          rounded_addr);

  return (void *) rounded_addr;
}
