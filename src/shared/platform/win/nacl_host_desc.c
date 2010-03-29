/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/shared/platform/nacl_find_addrsp.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/win/xlate_system_error.h"

#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"
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
int GetErrno() {
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
 * TODO(bsy): handle the !NACL_ABI_MAP_FIXED case.
 */

uintptr_t NaClHostDescMap(struct NaClHostDesc *d,
                          void                *start_addr,
                          size_t              len,
                          int                 prot,
                          int                 flags,
                          nacl_off64_t        offset) {
  uintptr_t retval;
  uintptr_t addr;
  HANDLE    hFile;
  HANDLE    hMap;
  DWORD     flProtect;
  DWORD     dwMaximumSizeHigh;
  DWORD     dwMaximumSizeLow;
  DWORD     dwDesiredAccess;
  uintptr_t map_result;
  size_t    chunk_offset;
  size_t    unmap_offset;
  size_t    chunk_size;

  if (NULL == d && 0 == (flags & NACL_ABI_MAP_ANONYMOUS)) {
    NaClLog(LOG_FATAL, "NaClHostDescMap: 'this' is NULL and not anon map\n");
  }
  addr = (uintptr_t) start_addr;
  prot &= (NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE);
  /* may be PROT_NONE too, just not PROT_EXEC */

  /*
   * Check that if FIXED, start_addr is not NULL.
   * Use platform free address space locator to set start_addr if NULL and
   * not FIXED.
   */
  if (0 == (flags & NACL_ABI_MAP_FIXED)) {
    /*
     * Not fixed, addr is a hint... which we ignore.  We cannot just
     * let windows pick, since we are mapping memmory in chunks of
     * 64-kB to permit piecewise unmapping.
     */
    if (!NaClFindAddressSpace(&addr, len)) {
      NaClLog(LOG_ERROR,
              "NaClHostDescMap: not fixed, and could not find space\n");
      return (uintptr_t) NACL_ABI_MAP_FAILED;
    }

    NaClLog(4,
            "NaClHostDescMap: NOT FIXED, found space at %"NACL_PRIxPTR"\n",
            addr);

    start_addr = (void *) addr;
  }

  flProtect = 0;
  dwDesiredAccess = 0;


  switch (prot) {
    case NACL_ABI_PROT_NONE:
      /*
       * PAGE_NOACCESS is not supported by CreateFileMapping, so
       * NACL_ABI_PROT_NONE memory must be free'd using VirtualFree.
       */
      flProtect = PAGE_NOACCESS;
      flags |= NACL_ABI_MAP_ANONYMOUS;
      break;
    case NACL_ABI_PROT_READ:
      flProtect |= PAGE_READONLY;
      dwDesiredAccess = FILE_MAP_READ;
      break;
    case NACL_ABI_PROT_WRITE:
      flProtect |= PAGE_WRITECOPY;
      dwDesiredAccess = FILE_MAP_COPY;
      break;
    case NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE:
      flProtect |= PAGE_WRITECOPY;
      dwDesiredAccess = FILE_MAP_COPY;
      break;
  }

  if (0 != (flags & NACL_ABI_MAP_ANONYMOUS)) {
    /*
     * anonymous memory must be free'able later via VirtualFree
     */
    if (0 != (flProtect & PAGE_WRITECOPY)) {
      flProtect &= ~PAGE_WRITECOPY;
      flProtect |= PAGE_READWRITE;
    }
    NaClLog(3, "NaClHostDescMap: anonymous mapping\n");
    for (chunk_offset = 0;
         chunk_offset < len;
         chunk_offset += NACL_MAP_PAGESIZE) {
      NaClLog(3,
              "NaClHostDescMap: VirtualAlloc(0x%08x,,%x,%x)\n",
              (void *) (addr + chunk_offset),
              MEM_COMMIT | MEM_RESERVE,
              flProtect);
      map_result = (uintptr_t) VirtualAlloc((void *) (addr + chunk_offset),
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
    NaClLog(3, "NaClHostDescMap: returning 0x%08"NACL_PRIxPTR"\n", start_addr);
    return (uintptr_t) start_addr;
  }

  hFile = (HANDLE) _get_osfhandle(d->d);
  dwMaximumSizeLow = 0;
  dwMaximumSizeHigh = 0;

  /*
   * if hFile is INVALID_HANDLE_VALUE, the memory is backed by the
   * system paging file.  why does it returns NULL instead of
   * INVALID_HANDLE_VALUE when there is an error?
   */
  hMap = CreateFileMapping(hFile,
                           NULL,
                           flProtect,
                           dwMaximumSizeHigh,
                           dwMaximumSizeLow,
                           NULL);
  if (NULL == hMap) {
    DWORD err = GetLastError();
    NaClLog(LOG_INFO,
            "NaclHostDescMap: CreateFileMapping failed: %d\n",
            err);
    return -NaClXlateSystemError(err);
  }

  retval = (uintptr_t) -NACL_ABI_EINVAL;

  for (chunk_offset = 0;
       chunk_offset < len;
       chunk_offset += NACL_MAP_PAGESIZE) {
    nacl_off64_t net_offset;
    uint32_t net_offset_high;
    uint32_t net_offset_low;

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
                                             (void *) (addr + chunk_offset));
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
  return retval;
}

int NaClHostDescUnmapCommon(void    *start_addr,
                            size_t  len,
                            int     fill_hole) {
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
    if (fill_hole) {
      if (VirtualAlloc((void *) (addr + off),
                       NACL_MAP_PAGESIZE,
                       MEM_RESERVE,
                       PAGE_READWRITE) == NULL) {
        return -NACL_ABI_E_MOVE_ADDRESS_SPACE;
      }
    }
  }
  return 0;
}

int NaClHostDescUnmapUnsafe(void    *start_addr,
                            size_t  len) {
  return NaClHostDescUnmapCommon(start_addr, len, 0);
}

int NaClHostDescUnmap(void    *start_addr,
                      size_t  len) {
  return NaClHostDescUnmapCommon(start_addr, len, 1);
}


int NaClHostDescOpen(struct NaClHostDesc  *d,
                     char const           *path,
                     int                  mode,
                     int                  perms) {
  int oflags;
  int pmode;

  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDescOpen: 'this' is NULL\n");
  }
  /*
   * Sanitize access modes.
   */
  if (0 != (mode & ~(NACL_ABI_O_ACCMODE | NACL_ABI_O_CREAT
                     | NACL_ABI_O_TRUNC | NACL_ABI_O_APPEND))) {
    return -NACL_ABI_EINVAL;
  }

  /*
   * Translate *x access mode to window's.
   */
  oflags = _O_BINARY;
  switch (mode & NACL_ABI_O_ACCMODE) {
    case NACL_ABI_O_RDONLY:
      oflags |= _O_RDONLY;
      break;
    case NACL_ABI_O_RDWR:
      oflags |= _O_RDWR;
      break;
    case NACL_ABI_O_WRONLY:
      oflags |= _O_WRONLY;
      break;
    default:
      NaClLog(LOG_ERROR,
              "NaClHostDescOpen: bad access mode 0x%x.\n",
              mode);
      return -NACL_ABI_EINVAL;
  }
  if (mode & NACL_ABI_O_CREAT) {
    oflags |= _O_CREAT;
  }
  if (mode & NACL_ABI_O_TRUNC) {
    oflags |= _O_TRUNC;
  }
  pmode = 0;
  if (0 != (perms & NACL_ABI_S_IRUSR)) {
    pmode |= _S_IREAD;
  }
  if (0 != (perms & NACL_ABI_S_IWUSR)) {
    pmode |= _S_IWRITE;
  }

  if (0 != _sopen_s(&d->d, path, oflags, _SH_DENYNO, pmode)) {
    return -GetErrno();
  }
  return 0;
}

int NaClHostDescPosixDup(struct NaClHostDesc  *d,
                         int                  posix_d,
                         int                  mode) {
  int host_desc;

  NaClLog(3, "NaClHostDescPosixDup(0x%08x, %d, 0%o)\n",
          (uintptr_t) d, posix_d, mode);
  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDescPosixDup: 'this' is NULL\n");
  }
  /*
   * Sanitize access modes.
   */
  if (0 != (mode & ~O_ACCMODE)) {
    return -NACL_ABI_EINVAL;
  }
  switch (mode & O_ACCMODE) {
    case NACL_ABI_O_RDONLY:
    case NACL_ABI_O_WRONLY:
    case NACL_ABI_O_RDWR:
      break;
    default:
      NaClLog(LOG_ERROR,
              "NaClHostDescOpen: bad access mode 0x%x.\n",
              mode);
      return -NACL_ABI_EINVAL;
  }

  host_desc = _dup(posix_d);
  if (-1 == host_desc) {
    return -GetErrno();
  }
  d->d = host_desc;
  return 0;
}

int NaClHostDescPosixTake(struct NaClHostDesc *d,
                          int                 posix_d,
                          int                 mode) {
  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDescPosixTake: 'this' is NULL\n");
  }
  /*
   * Sanitize access modes.
   */
  if (0 != (mode & ~O_ACCMODE)) {
    return -NACL_ABI_EINVAL;
  }
  switch (mode & O_ACCMODE) {
    case O_RDONLY:
    case O_WRONLY:
    case O_RDWR:
      break;
    default:
      NaClLog(LOG_ERROR,
              "NaClHostDescOpen: bad access mode 0x%x.\n",
              mode);
      return -NACL_ABI_EINVAL;
  }

  d->d = posix_d;
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

  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDescRead: 'this' is NULL\n");
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

  if (len < UINT_MAX) {
    actual_len = (unsigned int) len;
  } else {
    actual_len = UINT_MAX;
  }

  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDescWrite: 'this' is NULL\n");
  }
  if (-1 == (actual = _write(d->d, buf, actual_len))) {
    return -GetErrno();
  }
  return actual;
}

nacl_off64_t NaClHostDescSeek(struct NaClHostDesc  *d,
                              nacl_off64_t         offset,
                              int                  whence) {
  nacl_off64_t retval;

  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDescSeek: 'this' is NULL\n");
  }
  return (-1 == (retval = _lseeki64(d->d, offset, whence))) ? -errno : retval;
}

int NaClHostDescIoctl(struct NaClHostDesc *d,
                      int                 request,
                      void                *arg) {
  UNREFERENCED_PARAMETER(d);
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(arg);

  return -NACL_ABI_ENOSYS;
}

int NaClHostDescFstat(struct NaClHostDesc   *d,
                      nacl_host_stat_t      *nasp) {
  if (_fstat64(d->d, nasp) == -1) {
    return -GetErrno();
  }

  return 0;
}

int NaClHostDescClose(struct NaClHostDesc *d) {
  int retval;

  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDescClose: 'this' is NULL\n");
  }
  retval = _close(d->d);
  if (-1 != retval) {
    d->d = -1;
  }
  return (-1 == retval) ? -GetErrno() : retval;
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
