/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Service Runtime.  I/O Descriptor / Handle abstraction.  Memory
 * mapping using descriptors.
 */
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"

#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <share.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_find_addrsp.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/win/xlate_system_error.h"
#include "native_client/src/trusted/desc/nacl_desc_effector.h"

#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/internal_errno.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"

/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * The implementation of the host descriptor abstractions will
 * probably change.  In particularly, blocking I/O calls must be
 * interruptible in order to implement the address-space move
 * mechanism for mmap error recovery, and the it seems that the only
 * way that this would be feasible is to do the following: instead of
 * using the POSIX abstraction layer, do the I/O using Windows file
 * handles opened for asynchronous operations.  When a potentially
 * blocking system call (e.g., read or write) is performed, use
 * overlapped I/O via ReadFile/WriteFile to initiate the I/O operation
 * in a non-blocking manner, and use a separate event object, so that
 * the thread can, after initiating the I/O, perform
 * WaitForMultipleObjects on both I/O completion (event in the
 * OVERLAPPED structure) and on mmap-generated interrupts.  The event
 * can be signalled via SetEvent by any other thread that wish to
 * perform a safe mmap operation.
 *
 * When the safe mmap is to occur, all other application threads are
 * stopped (beware, of course, of the race condition where two threads
 * try to do mmap), and the remaining running thread performs
 * VirtualFree and MapViewOfFileEx.  If a thread (from some injected
 * DLL) puts some memory in the hole created by VirtualFree before the
 * MapViewOfFileEx runs, then we have to move the entire address space
 * to avoid allowing the untrusted NaCl app from touching this
 * innocent thread's memory.
 *
 * What this implies is that a mechanism must exist in order for the
 * mmapping thread to stop all other application threads, and this is
 * why the blocking syscalls must be interruptible.  When interrupted,
 * the thread that initiated the I/O must perform CancelIo and check,
 * via GetOverlappedResult, to see how much have completed, etc, then
 * put itself into a restartable state -- we might simply return EINTR
 * if no work has been dnoe and require libc to restart the syscall in
 * the SysV style, though it should be possible to just restart the
 * syscall in the BSD style -- and to signal the mmapping thread that
 * it is ready.
 *
 * Alternatively, these interrupted operations can return a private
 * version of EAGAIN, so that the code calling the host descriptor
 * (syscall handler) can quiesce the thread and restart the I/O
 * operation once the address space move is complete.
 *
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 */

/*
 * TODO(bsy, gregoryd): check that _get_errno is indeed a thread-safe way
 * to get the error from the last 'syscall' into the posix layer.
 */
int GetErrno(void) {
  int thread_errno;

  (void) _get_errno(&thread_errno);
  return NaClXlateErrno(thread_errno);
}

static INLINE size_t size_min(size_t a, size_t b) {
  return (a < b) ? a : b;
}

/*
 * The mapping and unmapping code work in 64K chunks rather than a
 * single large allocation because all of our uses will use 64K
 * chunks.  Higher level code keeps track of whether memory came from
 * VirtualAlloc or NaClHostDescMap, and will call the appropriate
 * deallocation functions.
 *
 * NB: if prot is NACL_ABI_PROT_NONE, then the memory should be
 * deallocated via VirtualFree as if it came from paging file rather
 * than via a file mapping object representing the paging file (and
 * thus UnmapViewOfFile).
 */

/*
 * out_flProtect == 0 means error, and the error string can be used
 * for a logging message (except for the cases that the caller should
 * be checking for).
 *
 * in parameters are all NACL_ABI_ values or bool (0/1).
 *
 * accmode may be NACL_ABI_O_RDONLY or NACL_ABI_O_RDWR, but not
 * NACL_ABI_O_WRONLY (see below).
 *
 * Caller should check for:
 *
 * - PROT_NONE -> special case handling,
 * - NACL_ABI_O_APPEND and PROT_WRITE -> EACCES,
 * - accmode is O_WRONLY -> EACCES,
 *
 * The interpretation here is that PROT_EXEC or PROT_WRITE implies
 * asking for PROT_READ, since most hardware behaves this way.  So if
 * the descriptor is O_WRONLY, we generally refuse.
 *
 * The file mapping object created by CreateFileMapping's flProtect
 * argument specifies the MAXIMUM protection, and MapViewOfFileEx will
 * request a lesser level of access.  We should always
 * CreateFileMapping with a high level of access so that
 * VirtualProtect (used by mprotect) can be used to turn on write when
 * the initial mmap had read-only mappings.
 *
 * BUG(bsy,phosek): we may need to retain the file object to serve
 * as backing store, as well as possibly for detecting dirty pages
 * by diffing.  This can cause unnecessary paging activity, however,
 * so invoking VirtualQuery to see if the page transitioned from
 * PAGE_WRITECOPY to PAGE_READWRITE has occurred might be better.
 * Additionally, VirtualQuery suggests using VirtualLock and then
 * QueryWorkingSetEx to check the Shared bit in the working set
 * information -- however, VirtualLock would itself cause the page
 * to fault in, so it's not clear that the benefits are huge
 * (performance trade-off between more syscalls vs memcmp).
 */
static void NaClflProtectAndDesiredAccessMap(int prot,
                                             int is_private,
                                             int accmode,
                                             DWORD *out_flProtect,
                                             DWORD *out_dwDesiredAccess,
                                             char const **out_msg) {
#define M(p,da,err) { p, da, err, #p, #da }
  static struct {
    DWORD flProtect;
    DWORD dwDesiredAccess;
    char const *err;
    char const *flProtect_str;
    char const *dwDesiredAccess_str;
  } table[] = {
    /* RDONLY */
    /* shared */
    /* PROT_NONE */
    M(0, 0, NULL),
    /* PROT_READ */
    M(PAGE_EXECUTE_READ, FILE_MAP_READ, NULL),
    /* PROT_WRITE */
    M(0, 0, "file open for read only; no shared write allowed"),
    /* PROT_READ | PROT_WRITE */
    M(0, 0, "file open for read only; no shared read/write allowed"),
    /* PROT_EXEC */
    M(PAGE_EXECUTE_READ, FILE_MAP_READ | FILE_MAP_EXECUTE, NULL),
    /* PROT_READ | PROT_EXEC */
    M(PAGE_EXECUTE_READ, FILE_MAP_READ | FILE_MAP_EXECUTE, NULL),
    /* PROT_WRITE | PROT_EXEC */
    M(0, 0, "file open for read only; no shared write/exec allowed"),
    /* PROT_READ | PROT_WRITE | PROT_EXEC */
    M(0, 0, "file open for read only; no shared read/write/exec allowed"),

    /* is_private */
    /* PROT_NONE */
    M(0, 0, NULL),
    /* PROT_READ */
    M(PAGE_EXECUTE_READ, FILE_MAP_READ, NULL),
    /* PROT_WRITE */
    M(PAGE_EXECUTE_READ, FILE_MAP_COPY, NULL),
    /* PROT_READ | PROT_WRITE */
    M(PAGE_EXECUTE_READ, FILE_MAP_COPY, NULL),

    /* PROT_EXEC */
    M(PAGE_EXECUTE_READ, FILE_MAP_READ | FILE_MAP_EXECUTE, NULL),
    /* PROT_READ | PROT_EXEC */
    M(PAGE_EXECUTE_READ, FILE_MAP_READ | FILE_MAP_EXECUTE, NULL),
    /* PROT_WRITE | PROT_EXEC */
    M(PAGE_EXECUTE_READ, FILE_MAP_COPY | FILE_MAP_EXECUTE, NULL),
    /* PROT_READ | PROT_WRITE | PROT_EXEC */
    M(PAGE_EXECUTE_READ, FILE_MAP_COPY | FILE_MAP_EXECUTE, NULL),

    /* RDWR */
    /* shared */
    /* PROT_NONE */
    M(0, 0, NULL ),
    /* PROT_READ */
    M(PAGE_EXECUTE_READWRITE, FILE_MAP_READ, NULL),
    /* PROT_WRITE */
    M(PAGE_EXECUTE_READWRITE, FILE_MAP_WRITE, NULL),
    /* PROT_READ | PROT_WRITE */
    M(PAGE_EXECUTE_READWRITE, FILE_MAP_WRITE, NULL),

    /* PROT_EXEC */
    M(PAGE_EXECUTE_READWRITE, FILE_MAP_READ | FILE_MAP_EXECUTE, NULL),
    /* PROT_READ | PROT_EXEC */
    M(PAGE_EXECUTE_READWRITE, FILE_MAP_READ | FILE_MAP_EXECUTE, NULL),
    /* PROT_WRITE | PROT_EXEC */
    M(PAGE_EXECUTE_READWRITE, FILE_MAP_WRITE | FILE_MAP_EXECUTE, NULL),
    /* PROT_READ | PROT_WRITE | PROT_EXEC */
    M(PAGE_EXECUTE_READWRITE, FILE_MAP_WRITE | FILE_MAP_EXECUTE, NULL),

    /* is_private */
    /* PROT_NONE */
    M(0, 0, NULL ),
    /* PROT_READ */
    M(PAGE_EXECUTE_READWRITE, FILE_MAP_READ, NULL),
    /* PROT_WRITE */
    M(PAGE_EXECUTE_READWRITE, FILE_MAP_COPY, NULL),
    /* PROT_READ | PROT_WRITE */
    M(PAGE_EXECUTE_READWRITE, FILE_MAP_COPY, NULL),

    /* PROT_EXEC */
    M(PAGE_EXECUTE_READWRITE, FILE_MAP_READ | FILE_MAP_EXECUTE, NULL),
    /* PROT_READ | PROT_EXEC */
    M(PAGE_EXECUTE_READWRITE, FILE_MAP_READ | FILE_MAP_EXECUTE, NULL),
    /* PROT_WRITE | PROT_EXEC */
    M(PAGE_EXECUTE_READWRITE, FILE_MAP_COPY | FILE_MAP_EXECUTE, NULL),
    /* PROT_READ | PROT_WRITE | PROT_EXEC */
    M(PAGE_EXECUTE_READWRITE, FILE_MAP_COPY | FILE_MAP_EXECUTE, NULL),
  };
#undef M

  size_t ix;

  NaClLog(3,
          "NaClflProtectAndDesiredAccessMap(prot 0x%x,"
          " priv 0x%x, accmode 0x%x, ...)\n",
          prot, is_private, accmode);

  NACL_COMPILE_TIME_ASSERT(NACL_ABI_O_RDONLY == 0);
  NACL_COMPILE_TIME_ASSERT(NACL_ABI_O_RDWR == 2);
  NACL_COMPILE_TIME_ASSERT(NACL_ABI_PROT_NONE == 0);
  NACL_COMPILE_TIME_ASSERT(NACL_ABI_PROT_READ == 1);
  NACL_COMPILE_TIME_ASSERT(NACL_ABI_PROT_WRITE == 2);
  NACL_COMPILE_TIME_ASSERT(NACL_ABI_PROT_EXEC == 4);

  CHECK(accmode != NACL_ABI_O_WRONLY);

  /*
   * NACL_ABI_O_RDONLY == 0, NACL_ABI_O_RDWR == 2, so multiplying by 8
   * yields a base separation of 8 for the 16 element subtable indexed
   * by the NACL_ABI_PROT_{READ|WRITE|EXEC} and is_private values.
   */
  ix = ((prot & NACL_ABI_PROT_MASK) +
        (is_private << 3) +
        ((accmode & NACL_ABI_O_ACCMODE) << 3));

  CHECK(ix < NACL_ARRAY_SIZE(table));  /* compiler should elide this */

  *out_flProtect = table[ix].flProtect;
  *out_dwDesiredAccess = table[ix].dwDesiredAccess;
  *out_msg = table[ix].err;

  NaClLog(3, "NaClflProtectAndDesiredAccessMap: %s %s\n",
          table[ix].flProtect_str, table[ix].dwDesiredAccess_str);
}

/*
 * Unfortunately, when the descriptor is imported via
 * NaClHostDescPosixTake or NaClHostDescPosixDup, the underlying file
 * handle may not have GENERIC_EXECUTE permission associated with it,
 * unlike the files open using NaClHostDescOpen.  This means that the
 * CreateFileMapping with flProtect that specifies PAGE_EXECUTE_* will
 * fail.  Since we don't know whether GENERIC_EXECUTE without doing a
 * query, we instead lazily determine the need by detecting the
 * CreateFileMapping error and retrying using a fallback flProtect
 * that does not have EXECUTE rights.  We record this in the
 * descriptor so that the next time we do not have to try with the
 * PAGE_EXECUTE_* and have it deterministically fail.
 *
 * This function is also used when mapping in anonymous memory.  We
 * assume that we never map anonymous executable memory -- mmap of
 * executable data is always from a file, and the page will be
 * non-writable -- and we ensure that anonymous memory is never
 * executable.
 */
static DWORD NaClflProtectRemoveExecute(DWORD flProtect) {
  switch (flProtect) {
    case PAGE_EXECUTE_READ:
      return PAGE_READONLY;
    case PAGE_EXECUTE_READWRITE:
      return PAGE_READWRITE;
  }
  return flProtect;
}

/*
 * TODO(mseaborn): Reduce duplication between this function and
 * nacl::Map()/NaClMap().
 */
uintptr_t NaClHostDescMap(struct NaClHostDesc *d,
                          struct NaClDescEffector *effp,
                          void                *start_addr,
                          size_t              len,
                          int                 prot,
                          int                 flags,
                          nacl_off64_t        offset) {
  uintptr_t retval;
  uintptr_t addr;
  int       desc_flags;
  HANDLE    hFile;
  HANDLE    hMap;
  int       retry_fallback;
  DWORD     flProtect;
  DWORD     dwDesiredAccess;
  char const *err_msg;
  DWORD     dwMaximumSizeHigh;
  DWORD     dwMaximumSizeLow;
  uintptr_t map_result;
  size_t    chunk_offset;
  size_t    unmap_offset;
  size_t    chunk_size;

  NaClLog(4,
          ("NaClHostDescMap(0x%08"NACL_PRIxPTR", 0x%08"NACL_PRIxPTR
           ", 0x%"NACL_PRIxS", 0x%x, 0x%x, 0x%016"NACL_PRIxNACL_OFF64")\n"),
          (uintptr_t) d, (uintptr_t) start_addr,
          len, prot, flags, offset);
  if (NULL == d && 0 == (flags & NACL_ABI_MAP_ANONYMOUS)) {
    NaClLog(LOG_FATAL, "NaClHostDescMap: 'this' is NULL and not anon map\n");
  }
  if (NULL != d && -1 == d->d) {
    NaClLog(LOG_FATAL, "NaClHostDescMap: already closed\n");
  }
  if ((0 == (flags & NACL_ABI_MAP_SHARED)) ==
      (0 == (flags & NACL_ABI_MAP_PRIVATE))) {
    NaClLog(LOG_FATAL,
            "NaClHostDescMap: exactly one of NACL_ABI_MAP_SHARED"
            " and NACL_ABI_MAP_PRIVATE must be set.\n");
  }
  addr = (uintptr_t) start_addr;
  prot &= NACL_ABI_PROT_MASK;

  /*
   * Check that if FIXED, start_addr is not NULL.
   * Use platform free address space locator to set start_addr if NULL and
   * not FIXED.
   */
  if (0 == (flags & NACL_ABI_MAP_FIXED)) {
    /*
     * Not fixed, addr is a hint... which we ignore.  We cannot just
     * let windows pick, since we are mapping memory in chunks of
     * 64-kB to permit piecewise unmapping.
     */
    if (!NaClFindAddressSpace(&addr, len)) {
      NaClLog(LOG_ERROR,
              "NaClHostDescMap: not fixed, and could not find space\n");
      return (uintptr_t) -NACL_ABI_ENOMEM;
    }

    NaClLog(4,
            "NaClHostDescMap: NOT FIXED, found space at %"NACL_PRIxPTR"\n",
            addr);

    start_addr = (void *) addr;
  }

  flProtect = 0;
  dwDesiredAccess = 0;

  if (NULL == d) {
    desc_flags = NACL_ABI_O_RDWR;
  } else {
    desc_flags = d->flags;
  }

  if (0 != (desc_flags & NACL_ABI_O_APPEND) &&
      0 != (prot & NACL_ABI_PROT_WRITE)) {
    return (uintptr_t) -NACL_ABI_EACCES;
  }
  if (NACL_ABI_PROT_NONE == prot) {
    flProtect = PAGE_NOACCESS;
    flags |= NACL_ABI_MAP_ANONYMOUS;
  } else {
    NaClflProtectAndDesiredAccessMap(prot,
                                     0 != (flags & NACL_ABI_MAP_PRIVATE),
                                     (desc_flags & NACL_ABI_O_ACCMODE),
                                     &flProtect,
                                     &dwDesiredAccess,
                                     &err_msg);
    if (0 == flProtect) {
      NaClLog(3, "NaClHostDescMap: %s\n", err_msg);
      return (uintptr_t) -NACL_ABI_EACCES;
    }
    NaClLog(3, "flProtect 0x%x, dwDesiredAccess 0x%x\n",
            flProtect, dwDesiredAccess);
  }

  if (0 != (flags & NACL_ABI_MAP_ANONYMOUS)) {
    /*
     * anonymous memory must be free'able later via VirtualFree
     */
    NaClLog(3, "NaClHostDescMap: anonymous mapping\n");

    flProtect = NaClflProtectRemoveExecute(flProtect);

    for (chunk_offset = 0;
         chunk_offset < len;
         chunk_offset += NACL_MAP_PAGESIZE) {
      uintptr_t chunk_addr = addr + chunk_offset;

      (*effp->vtbl->UnmapMemory)(effp, chunk_addr, NACL_MAP_PAGESIZE);

      NaClLog(3,
              "NaClHostDescMap: VirtualAlloc(0x%08x,,%x,%x)\n",
              (void *) (addr + chunk_offset),
              MEM_COMMIT | MEM_RESERVE,
              flProtect);
      map_result = (uintptr_t) VirtualAlloc((void *) chunk_addr,
                                            NACL_MAP_PAGESIZE,
                                            MEM_COMMIT | MEM_RESERVE,
                                            flProtect);
      if (map_result != addr + chunk_offset) {
        NaClLog(LOG_FATAL,
                ("Could not VirtualAlloc anonymous memory at"
                 " addr 0x%08x with prot %x\n"),
                addr + chunk_offset, flProtect);
      }
    }
    NaClLog(3, "NaClHostDescMap: (anon) returning 0x%08"NACL_PRIxPTR"\n",
            start_addr);
    return (uintptr_t) start_addr;
  }

  hFile = (HANDLE) _get_osfhandle(d->d);
  dwMaximumSizeLow = 0;
  dwMaximumSizeHigh = 0;

  if (0 != d->flProtect) {
    flProtect = d->flProtect;
    retry_fallback = 0;
  } else {
    retry_fallback = 1;
  }

  do {
    /*
     * If hFile is INVALID_HANDLE_VALUE, the memory is backed by the
     * system paging file.  Why does it returns NULL instead of
     * INVALID_HANDLE_VALUE when there is an error?
     */
    hMap = CreateFileMapping(hFile,
                             NULL,
                             flProtect,
                             dwMaximumSizeHigh,
                             dwMaximumSizeLow,
                             NULL);
    if (NULL == hMap && retry_fallback != 0) {
      NaClLog(3,
              "NaClHostDescMap: CreateFileMapping failed, retrying without"
              " execute permission.  Original flProtect 0x%x\n", flProtect);
      flProtect = NaClflProtectRemoveExecute(flProtect);
      NaClLog(3,
              "NaClHostDescMap: fallback flProtect 0x%x\n", flProtect);
    } else {
      /* remember successful flProtect used */
      d->flProtect = flProtect;
    }
  } while (--retry_fallback >= 0);

  if (NULL == hMap) {
    DWORD err = GetLastError();
    NaClLog(LOG_INFO,
            "NaClHostDescMap: CreateFileMapping failed: %d\n",
            err);
    return -NaClXlateSystemError(err);
  }
  NaClLog(3, "NaClHostDescMap: CreateFileMapping got handle %d\n", (int) hMap);
  NaClLog(3, "NaClHostDescMap: dwDesiredAccess 0x%x\n", dwDesiredAccess);

  retval = (uintptr_t) -NACL_ABI_EINVAL;

  for (chunk_offset = 0;
       chunk_offset < len;
       chunk_offset += NACL_MAP_PAGESIZE) {
    uintptr_t chunk_addr = addr + chunk_offset;
    nacl_off64_t net_offset;
    uint32_t net_offset_high;
    uint32_t net_offset_low;

    (*effp->vtbl->UnmapMemory)(effp, chunk_addr, NACL_MAP_PAGESIZE);

    chunk_size = size_min(len - chunk_offset, NACL_MAP_PAGESIZE);
    /* in case MapViewOfFile cares that we exceed the file size */
    net_offset = offset + chunk_offset;
    net_offset_high = (uint32_t) (net_offset >> 32);
    net_offset_low = (uint32_t) net_offset;
    map_result = (uintptr_t) MapViewOfFileEx(hMap,
                                             dwDesiredAccess,
                                             net_offset_high,
                                             net_offset_low,
                                             chunk_size,
                                             (void *) chunk_addr);
    if ((addr + chunk_offset) != map_result) {
      DWORD err = GetLastError();
      NaClLog(LOG_INFO,
              "MapViewOfFileEx failed at 0x%08x, got 0x%08x, err %x\n",
              addr + chunk_offset,
              map_result,
              err);
      for (unmap_offset = 0;
           unmap_offset < chunk_offset;
           unmap_offset += NACL_MAP_PAGESIZE) {
        (void) UnmapViewOfFile((void *) (addr + unmap_offset));
      }
      retval = (uintptr_t) -NaClXlateSystemError(err);
      goto cleanup;
    }
  }
  retval = (uintptr_t) start_addr;
cleanup:
  (void) CloseHandle(hMap);
  NaClLog(3, "NaClHostDescMap: returning %"NACL_PRIxPTR"\n", retval);
  return retval;
}

int NaClHostDescUnmapUnsafe(void    *start_addr,
                            size_t  len) {
  uintptr_t addr;
  size_t    off;

  addr = (uintptr_t) start_addr;

  for (off = 0; off < len; off += NACL_MAP_PAGESIZE) {
    if (!UnmapViewOfFile((void *) (addr + off))) {
      NaClLog(LOG_ERROR,
              "NaClHostDescUnmap: UnmapViewOfFile(0x%08x) failed\n",
              addr + off);
      return -NACL_ABI_EINVAL;
    }
  }
  return 0;
}

int NaClHostDescOpen(struct NaClHostDesc  *d,
                     char const           *path,
                     int                  flags,
                     int                  perms) {
  DWORD dwDesiredAccess;
  DWORD dwCreationDisposition;
  DWORD dwFlagsAndAttributes;
  int oflags;
  int truncate_after_open = 0;
  HANDLE hFile;
  DWORD err;

  /*
   * TODO(bsy): do something reasonable with perms.  In particular,
   * Windows does support read-only files, so if (perms &
   * NACL_ABI_IRWXU) == NACL_ABI_S_IRUSR, we could create the file
   * with FILE_ATTRIBUTE_READONLY.  Since only test code is allowed to
   * open files, this is low priority.
   */
  UNREFERENCED_PARAMETER(perms);
  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDescOpen: 'this' is NULL\n");
  }
  /*
   * Sanitize access flags.
   */
  if (0 != (flags & ~NACL_ALLOWED_OPEN_FLAGS)) {
    return -NACL_ABI_EINVAL;
  }

  switch (flags & NACL_ABI_O_ACCMODE) {
    case NACL_ABI_O_RDONLY:
      dwDesiredAccess = GENERIC_READ | GENERIC_EXECUTE;
      oflags = _O_RDONLY | _O_BINARY;
      break;
    case NACL_ABI_O_RDWR:
      oflags = _O_RDWR | _O_BINARY;
      dwDesiredAccess = GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE;
      break;
    case NACL_ABI_O_WRONLY:  /* Enforced in the Read call */
      oflags = _O_WRONLY | _O_BINARY;
      dwDesiredAccess = GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE;
      break;
    default:
      NaClLog(LOG_ERROR,
              "NaClHostDescOpen: bad access flags 0x%x.\n",
              flags);
      return -NACL_ABI_EINVAL;
  }
  /*
   * Possibly make the file read-only.  The file attribute only
   * applies if the file is created; if it pre-exists, the attributes
   * from the file is combined with the FILE_FLAG_* values.
   */
  if (0 == (perms & NACL_ABI_S_IWUSR)) {
    dwFlagsAndAttributes = (FILE_ATTRIBUTE_READONLY |
                            FILE_FLAG_POSIX_SEMANTICS);
  } else {
    dwFlagsAndAttributes = (FILE_ATTRIBUTE_NORMAL |
                            FILE_FLAG_POSIX_SEMANTICS);
  }
  /*
   * Postcondition: flags & NACL_ABI_O_ACCMODE is one of the three
   * allowed values.
   */
  switch (flags & (NACL_ABI_O_CREAT | NACL_ABI_O_TRUNC)) {
    case 0:
      dwCreationDisposition = OPEN_EXISTING;
      break;
    case NACL_ABI_O_CREAT:
      dwCreationDisposition = OPEN_ALWAYS;
      break;
    case NACL_ABI_O_TRUNC:
      dwCreationDisposition = OPEN_EXISTING;
      truncate_after_open = 1;
      break;
    case NACL_ABI_O_CREAT | NACL_ABI_O_TRUNC:
      dwCreationDisposition = OPEN_ALWAYS;
      truncate_after_open = 1;
  }
  if (0 != (flags & NACL_ABI_O_APPEND)) {
    oflags |= _O_APPEND;
  }

  hFile = CreateFileA(path, dwDesiredAccess,
                      (FILE_SHARE_DELETE |
                       FILE_SHARE_READ |
                       FILE_SHARE_WRITE),
                      NULL,
                      dwCreationDisposition,
                      dwFlagsAndAttributes,
                      NULL);
  if (INVALID_HANDLE_VALUE == hFile) {
    err = GetLastError();
    NaClLog(3, "NaClHostDescOpen: CreateFile failed %d\n", err);
    return -NaClXlateSystemError(err);
  }
  if (truncate_after_open &&
      NACL_ABI_O_RDONLY != (flags & NACL_ABI_O_ACCMODE)) {
    (void) SetEndOfFile(hFile);
  }
  d->d = _open_osfhandle((intptr_t) hFile, oflags);
  /*
   * oflags _O_APPEND, _O_RDONLY, and _O_TEXT are meaningful; unclear
   * whether _O_RDWR, _O_WRONLY, etc has any effect.
   */
  if (-1 == d->d) {
    NaClLog(LOG_FATAL, "NaClHostDescOpen failed: err %d\n",
            GetLastError());
  }
  d->flags = flags;
  d->flProtect = 0;
  return 0;
}

int NaClHostDescPosixDup(struct NaClHostDesc  *d,
                         int                  posix_d,
                         int                  flags) {
  int host_desc;

  NaClLog(3, "NaClHostDescPosixDup(0x%08x, %d, 0%o)\n",
          (uintptr_t) d, posix_d, flags);
  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDescPosixDup: 'this' is NULL\n");
  }
  /*
   * Sanitize access flags.
   */
  if (0 != (flags & ~NACL_ALLOWED_OPEN_FLAGS)) {
    return -NACL_ABI_EINVAL;
  }
  switch (flags & NACL_ABI_O_ACCMODE) {
    case NACL_ABI_O_RDONLY:
    case NACL_ABI_O_WRONLY:
    case NACL_ABI_O_RDWR:
      break;
    default:
      NaClLog(LOG_ERROR,
              "NaClHostDescOpen: bad access flags 0x%x.\n",
              flags);
      return -NACL_ABI_EINVAL;
  }

  host_desc = _dup(posix_d);
  if (-1 == host_desc) {
    return -GetErrno();
  }
  d->d = host_desc;
  d->flags = flags;
  d->flProtect = 0;
  return 0;
}

int NaClHostDescPosixTake(struct NaClHostDesc *d,
                          int                 posix_d,
                          int                 flags) {
  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDescPosixTake: 'this' is NULL\n");
  }
  /*
   * Sanitize access flags.
   */
  if (0 != (flags & ~NACL_ALLOWED_OPEN_FLAGS)) {
    return -NACL_ABI_EINVAL;
  }
  switch (flags & NACL_ABI_O_ACCMODE) {
    case NACL_ABI_O_RDONLY:
    case NACL_ABI_O_WRONLY:
    case NACL_ABI_O_RDWR:
      break;
    default:
      NaClLog(LOG_ERROR,
              "NaClHostDescOpen: bad access flags 0x%x.\n",
              flags);
      return -NACL_ABI_EINVAL;
  }

  d->d = posix_d;
  d->flags = flags;
  d->flProtect = 0;
  return 0;
}

ssize_t NaClHostDescRead(struct NaClHostDesc  *d,
                         void                 *buf,
                         size_t               len) {
  ssize_t actual;

  /* Windows _read only supports uint, so we need
   * to clamp the length. */
  unsigned int actual_len;

  if (len < UINT_MAX) {
    actual_len = (unsigned int) len;
  } else {
    actual_len = UINT_MAX;
  }

  NaClHostDescCheckValidity("NaClHostDescRead", d);
  if (NACL_ABI_O_WRONLY == (d->flags & NACL_ABI_O_ACCMODE)) {
    NaClLog(3, "NaClHostDescRead: WRONLY file\n");
    return -NACL_ABI_EBADF;
  }
  if (-1 == (actual =_read(d->d, buf, actual_len))) {
    return -GetErrno();
  }
  return actual;
}

ssize_t NaClHostDescWrite(struct NaClHostDesc *d,
                          void const          *buf,
                          size_t              len) {
  ssize_t actual;

  /* Windows _write only supports uint, so we need
   * to clamp the length.
   */
  unsigned int actual_len;

  if (NACL_ABI_O_RDONLY == (d->flags & NACL_ABI_O_ACCMODE)) {
    NaClLog(3, "NaClHostDescWrite: RDONLY file\n");
    return -NACL_ABI_EBADF;
  }
  if (len < UINT_MAX) {
    actual_len = (unsigned int) len;
  } else {
    actual_len = UINT_MAX;
  }

  NaClHostDescCheckValidity("NaClHostDescWrite", d);
  if (-1 == (actual = _write(d->d, buf, actual_len))) {
    return -GetErrno();
  }
  return actual;
}

nacl_off64_t NaClHostDescSeek(struct NaClHostDesc  *d,
                              nacl_off64_t         offset,
                              int                  whence) {
  nacl_off64_t retval;

  NaClHostDescCheckValidity("NaClHostDescSeek", d);
  if (NACL_ABI_O_APPEND == (d->flags & NACL_ABI_O_APPEND)) {
    NaClLog(4, "NaClHostDescSeek: APPEND file emulation, seeks are no-ops\n");
    return 0;
  }
  return (-1 == (retval = _lseeki64(d->d, offset, whence))) ? -errno : retval;
}

int NaClHostDescIoctl(struct NaClHostDesc *d,
                      int                 request,
                      void                *arg) {
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(arg);

  NaClHostDescCheckValidity("NaClHostDescIoctl", d);
  return -NACL_ABI_ENOSYS;
}

int NaClHostDescFstat(struct NaClHostDesc   *d,
                      nacl_host_stat_t      *nasp) {
  NaClHostDescCheckValidity("NaClHostDescFstat", d);
  if (_fstat64(d->d, nasp) == -1) {
    return -GetErrno();
  }

  return 0;
}

int NaClHostDescClose(struct NaClHostDesc *d) {
  int retval;

  NaClHostDescCheckValidity("NaClHostDescClose", d);
  if (-1 != d->d) {
    retval = _close(d->d);
    if (-1 == retval) {
      return -GetErrno();
    }
    d->d = -1;
  }
  return 0;
}

/*
 * This is not a host descriptor function, but is closely related to
 * fstat and should behave similarly.
 */
int NaClHostDescStat(char const       *host_os_pathname,
                     nacl_host_stat_t *nhsp) {
  if (_stati64(host_os_pathname, nhsp) == -1) {
    return -GetErrno();
  }

  return 0;
}
