/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Service Runtime.  I/O Descriptor / Handle abstraction.  Memory
 * mapping using descriptors.
 *
 * Note that we avoid using the thread-specific data / thread local
 * storage access to the "errno" variable, and instead use the raw
 * system call return interface of small negative numbers as errors.
 */

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/include/portability.h"

#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"

#if NACL_LINUX
# define PREAD pread64
# define PWRITE pwrite64
#elif NACL_OSX
# define PREAD pread
# define PWRITE pwrite
#else
# error "Which POSIX OS?"
#endif

/*
 * Map our ABI to the host OS's ABI.  On linux, this should be a big no-op.
 */
static INLINE int NaClMapOpenFlags(int nacl_flags) {
  int host_os_flags;

  nacl_flags &= (NACL_ABI_O_ACCMODE | NACL_ABI_O_CREAT
                 | NACL_ABI_O_TRUNC | NACL_ABI_O_APPEND);

  host_os_flags = 0;
#define C(H) case NACL_ABI_ ## H: \
  host_os_flags |= H;             \
  break;
  switch (nacl_flags & NACL_ABI_O_ACCMODE) {
    C(O_RDONLY);
    C(O_WRONLY);
    C(O_RDWR);
  }
#undef C
#define M(H) do { \
    if (0 != (nacl_flags & NACL_ABI_ ## H)) {   \
      host_os_flags |= H;                       \
    }                                           \
  } while (0)
  M(O_CREAT);
  M(O_TRUNC);
  M(O_APPEND);
#undef M
  return host_os_flags;
}

static INLINE int NaClMapOpenPerm(int nacl_perm) {
  int host_os_perm;

  host_os_perm = 0;
#define M(H) do { \
    if (0 != (nacl_perm & NACL_ABI_ ## H)) { \
      host_os_perm |= H; \
    } \
  } while (0)
  M(S_IRUSR);
  M(S_IWUSR);
#undef M
  return host_os_perm;
}

static INLINE int NaClMapFlagMap(int nacl_map_flags) {
  int host_os_flags;

  host_os_flags = 0;
#define M(H) do { \
    if (0 != (nacl_map_flags & NACL_ABI_ ## H)) { \
      host_os_flags |= H; \
    } \
  } while (0)
  M(MAP_SHARED);
  M(MAP_PRIVATE);
  M(MAP_FIXED);
  M(MAP_ANONYMOUS);
#undef M

  return host_os_flags;
}

/*
 * TODO(bsy): handle the !NACL_ABI_MAP_FIXED case.
 */
uintptr_t NaClHostDescMap(struct NaClHostDesc *d,
                          struct NaClDescEffector *effp,
                          void                *start_addr,
                          size_t              len,
                          int                 prot,
                          int                 flags,
                          nacl_off64_t        offset) {
  int   desc;
  void  *map_addr;
  int   host_prot;
  int   tmp_prot;
  int   host_flags;
  int   need_exec;
  UNREFERENCED_PARAMETER(effp);

  NaClLog(4,
          ("NaClHostDescMap(0x%08"NACL_PRIxPTR", "
           "0x%08"NACL_PRIxPTR", 0x%08"NACL_PRIxS", "
           "0x%x, 0x%x, 0x%08"NACL_PRIx64")\n"),
          (uintptr_t) d,
          (uintptr_t) start_addr,
          len,
          prot,
          flags,
          (int64_t) offset);
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

  prot &= NACL_ABI_PROT_MASK;

  if (flags & NACL_ABI_MAP_ANONYMOUS) {
    desc = -1;
  } else {
    desc = d->d;
  }
  /*
   * Translate prot, flags to host_prot, host_flags.
   */
  host_prot = NaClProtMap(prot);
  host_flags = NaClMapFlagMap(flags);

  NaClLog(4, "NaClHostDescMap: host_prot 0x%x, host_flags 0x%x\n",
          host_prot, host_flags);

  /*
   * In chromium-os, the /dev/shm and the user partition (where
   * installed apps live) are mounted no-exec, and a special
   * modification was made to the chromium-os version of the Linux
   * kernel to allow mmap to use files as backing store with
   * PROT_EXEC. The standard mmap code path will fail mmap requests
   * that ask for PROT_EXEC, but mprotect will allow chaning the
   * permissions later. This retains most of the defense-in-depth
   * property of disallowing PROT_EXEC in mmap, but enables the use
   * case of getting executable code from a file without copying.
   *
   * See https://code.google.com/p/chromium/issues/detail?id=202321
   * for details of the chromium-os change.
   */
  tmp_prot = host_prot & ~PROT_EXEC;
  need_exec = (0 != (PROT_EXEC & host_prot));
  map_addr = mmap(start_addr, len, tmp_prot, host_flags, desc, offset);
  if (need_exec && MAP_FAILED != map_addr) {
    if (0 != mprotect(map_addr, len, host_prot)) {
      /*
       * Not being able to turn on PROT_EXEC is fatal: we have already
       * replaced the original mapping -- restoring them would be too
       * painful.  Without scanning /proc (disallowed by outer
       * sandbox) or Mach's vm_region call, there is no way
       * simple/direct to figure out what was there before.  On Linux
       * we could have mremap'd the old memory elsewhere, but still
       * would require probing to find the contiguous memory segments
       * within the original address range.  And restoring dirtied
       * pages on OSX the mappings for which had disappeared may well
       * be impossible (getting clean copies of the pages is feasible,
       * but insufficient).
       */
      NaClLog(LOG_FATAL,
              "NaClHostDescMap: mprotect to turn on PROT_EXEC failed,"
              " errno %d\n", errno);
    }
  }

  NaClLog(4, "NaClHostDescMap: mmap returned %"NACL_PRIxPTR"\n",
          (uintptr_t) map_addr);

  if (MAP_FAILED == map_addr) {
    NaClLog(LOG_INFO,
            ("NaClHostDescMap: "
             "mmap(0x%08"NACL_PRIxPTR", 0x%"NACL_PRIxS", "
             "0x%x, 0x%x, 0x%d, 0x%"NACL_PRIx64")"
             " failed, errno %d.\n"),
            (uintptr_t) start_addr, len, host_prot, host_flags, desc,
            (int64_t) offset,
            errno);
    return -NaClXlateErrno(errno);
  }
  if (0 != (flags & NACL_ABI_MAP_FIXED) && map_addr != start_addr) {
    NaClLog(LOG_FATAL,
            ("NaClHostDescMap: mmap with MAP_FIXED not fixed:"
             " returned 0x%08"NACL_PRIxPTR" instead of 0x%08"NACL_PRIxPTR"\n"),
            (uintptr_t) map_addr,
            (uintptr_t) start_addr);
  }
  NaClLog(4, "NaClHostDescMap: returning 0x%08"NACL_PRIxPTR"\n",
          (uintptr_t) map_addr);

  return (uintptr_t) map_addr;
}

int NaClHostDescUnmapUnsafe(void *start_addr, size_t len) {
  return (0 == munmap(start_addr, len)) ? 0 : -errno;
}

static int NaClHostDescCtor(struct NaClHostDesc  *d,
                            int fd,
                            int flags) {
  d->d = fd;
  d->flags = flags;
  NaClLog(3, "NaClHostDescCtor: success.\n");
  return 0;
}

int NaClHostDescOpen(struct NaClHostDesc  *d,
                     char const           *path,
                     int                  flags,
                     int                  mode) {
  int host_desc;
  nacl_host_stat_t stbuf;
  int posix_flags;

  NaClLog(3, "NaClHostDescOpen(0x%08"NACL_PRIxPTR", %s, 0x%x, 0x%x)\n",
          (uintptr_t) d, path, flags, mode);
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
    case NACL_ABI_O_WRONLY:
    case NACL_ABI_O_RDWR:
      break;
    default:
      NaClLog(LOG_ERROR,
              "NaClHostDescOpen: bad access flags 0x%x.\n",
              flags);
      return -NACL_ABI_EINVAL;
  }

  posix_flags = NaClMapOpenFlags(flags);
#if NACL_LINUX
  posix_flags |= O_LARGEFILE;
#endif
  mode = NaClMapOpenPerm(mode);

  NaClLog(3, "NaClHostDescOpen: invoking POSIX open(%s,0x%x,0%o)\n",
          path, posix_flags, mode);
  host_desc = open(path, posix_flags, mode);
  NaClLog(3, "NaClHostDescOpen: got descriptor %d\n", host_desc);
  if (-1 == host_desc) {
    NaClLog(2, "NaClHostDescOpen: open returned -1, errno %d\n", errno);
    return -NaClXlateErrno(errno);
  }
  if (-1 ==
#if NACL_LINUX
      fstat64(host_desc, &stbuf)
#else
      fstat(host_desc, &stbuf)
#endif
      ) {
    NaClLog(LOG_ERROR,
            "NaClHostDescOpen: fstat failed?!?  errno %d\n", errno);
    (void) close(host_desc);
    return -NaClXlateErrno(errno);
  }
  if (!S_ISREG(stbuf.st_mode)) {
    NaClLog(LOG_INFO,
            "NaClHostDescOpen: file type 0x%x, not regular\n", stbuf.st_mode);
    (void) close(host_desc);
    /* cannot access anything other than a real file */
    return -NACL_ABI_EPERM;
  }
  return NaClHostDescCtor(d, host_desc, flags);
}

int NaClHostDescPosixDup(struct NaClHostDesc  *d,
                         int                  posix_d,
                         int                  flags) {
  int host_desc;

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
              "NaClHostDescPosixDup: bad access flags 0x%x.\n",
              flags);
      return -NACL_ABI_EINVAL;
  }

  host_desc = dup(posix_d);
  if (-1 == host_desc) {
    return -NACL_ABI_EINVAL;
  }
  d->d = host_desc;
  d->flags = flags;
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
              "NaClHostDescPosixTake: bad access flags 0x%x.\n",
              flags);
      return -NACL_ABI_EINVAL;
  }

  d->d = posix_d;
  d->flags = flags;
  return 0;
}

ssize_t NaClHostDescRead(struct NaClHostDesc  *d,
                         void                 *buf,
                         size_t               len) {
  ssize_t retval;

  NaClHostDescCheckValidity("NaClHostDescRead", d);
  if (NACL_ABI_O_WRONLY == (d->flags & NACL_ABI_O_ACCMODE)) {
    NaClLog(3, "NaClHostDescRead: WRONLY file\n");
    return -NACL_ABI_EBADF;
  }
  return ((-1 == (retval = read(d->d, buf, len)))
          ? -NaClXlateErrno(errno) : retval);
}

ssize_t NaClHostDescWrite(struct NaClHostDesc *d,
                          void const          *buf,
                          size_t              len) {
  ssize_t retval;

  NaClHostDescCheckValidity("NaClHostDescWrite", d);
  if (NACL_ABI_O_RDONLY == (d->flags & NACL_ABI_O_ACCMODE)) {
    NaClLog(3, "NaClHostDescWrite: RDONLY file\n");
    return -NACL_ABI_EBADF;
  }
  return ((-1 == (retval = write(d->d, buf, len)))
          ? -NaClXlateErrno(errno) : retval);
}

nacl_off64_t NaClHostDescSeek(struct NaClHostDesc  *d,
                              nacl_off64_t         offset,
                              int                  whence) {
  nacl_off64_t retval;

  NaClHostDescCheckValidity("NaClHostDescSeek", d);
#if NACL_LINUX
  return ((-1 == (retval = lseek64(d->d, offset, whence)))
          ? -NaClXlateErrno(errno) : retval);
#elif NACL_OSX
  return ((-1 == (retval = lseek(d->d, offset, whence)))
          ? -NaClXlateErrno(errno) : retval);
#else
# error "What Unix-like OS is this?"
#endif
}

ssize_t NaClHostDescPRead(struct NaClHostDesc *d,
                          void *buf,
                          size_t len,
                          nacl_off64_t offset) {
  ssize_t retval;

  NaClHostDescCheckValidity("NaClHostDescPRead", d);
  if (NACL_ABI_O_WRONLY == (d->flags & NACL_ABI_O_ACCMODE)) {
    NaClLog(3, "NaClHostDescPRead: WRONLY file\n");
    return -NACL_ABI_EBADF;
  }
  return ((-1 == (retval = PREAD(d->d, buf, len, offset)))
          ? -NaClXlateErrno(errno) : retval);
}

ssize_t NaClHostDescPWrite(struct NaClHostDesc *d,
                           void const *buf,
                           size_t len,
                           nacl_off64_t offset) {
  ssize_t retval;

  NaClHostDescCheckValidity("NaClHostDescPWrite", d);
  if (NACL_ABI_O_RDONLY == (d->flags & NACL_ABI_O_ACCMODE)) {
    NaClLog(3, "NaClHostDescPWrite: RDONLY file\n");
    return -NACL_ABI_EBADF;
  }
#if NACL_OSX
  /*
   * OSX's interpretation of what the POSIX standard requires differs
   * from Linux.  On OSX, pwrite using a descriptor that was opened
   * with O_APPEND will not append, but write to the offset specified
   * by the pread formal parameter.  On Linux, the O_APPEND-induced
   * seek to the end wins.  We standardize on Linux behavior.  By just
   * using the write syscall, we ensure that the
   * seek-to-end-before-write semantics apply.
   */
  if (0 != (d->flags & NACL_ABI_O_APPEND)) {
    return ((-1 == (retval = write(d->d, buf, len)))
            ? -NaClXlateErrno(errno) : retval);
  }
#endif
  return ((-1 == (retval = PWRITE(d->d, buf, len, offset)))
          ? -NaClXlateErrno(errno) : retval);
}


int NaClHostDescIoctl(struct NaClHostDesc *d,
                      int                 request,
                      void                *arg) {
  UNREFERENCED_PARAMETER(request);
  UNREFERENCED_PARAMETER(arg);

  NaClHostDescCheckValidity("NaClHostDescIoctl", d);
  return -NACL_ABI_ENOSYS;
}

/*
 * See NaClHostDescStat below.
 */
int NaClHostDescFstat(struct NaClHostDesc  *d,
                      nacl_host_stat_t     *nhsp) {
  NaClHostDescCheckValidity("NaClHostDescFstat", d);
#if NACL_LINUX
  if (fstat64(d->d, nhsp) == -1) {
    return -errno;
  }
#elif NACL_OSX
  if (fstat(d->d, nhsp) == -1) {
    return -errno;
  }
#else
# error "What OS?"
#endif

  return 0;
}

int NaClHostDescClose(struct NaClHostDesc *d) {
  int retval;

  NaClHostDescCheckValidity("NaClHostDescClose", d);
  retval = close(d->d);
  if (-1 != retval) {
    d->d = -1;
  }
  return (-1 == retval) ? -NaClXlateErrno(errno) : retval;
}

/*
 * This is not a host descriptor function, but is closely related to
 * fstat and should behave similarly.
 */
int NaClHostDescStat(char const       *host_os_pathname,
                     nacl_host_stat_t *nhsp) {

#if NACL_LINUX
  if (stat64(host_os_pathname, nhsp) == -1) {
    return -errno;
  }
#elif NACL_OSX
  if (stat(host_os_pathname, nhsp) == -1) {
    return -errno;
  }
#else
# error "What OS?"
#endif

  return 0;
}
