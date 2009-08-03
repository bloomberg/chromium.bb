/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NaCl Service Runtime.  I/O Descriptor / Handle abstraction.  Memory
 * mapping using descriptors.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_HOST_DESC_H__
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_HOST_DESC_H__

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/platform/nacl_sync.h"

#if NACL_LINUX || NACL_OSX
# include "native_client/src/trusted/platform/linux/nacl_host_desc_types.h"
#elif NACL_WINDOWS
# include "native_client/src/trusted/platform/win/nacl_host_desc_types.h"
#endif



/*
 * see NACL_MAP_PAGESIZE from nacl_config.h; map operations must be aligned
 */

EXTERN_C_BEGIN

struct nacl_abi_stat;
struct NaClHostDesc;

static INLINE int NaClIsNegErrno(int val) {
  /*
   * On 64-bit Linux, the app has the entire 32-bit address space
   * (kernel no longer occupies the top 1G), so what is an errno and
   * what is an address is trickier: we require that our NACL_ABI_
   * errno values be at most 64K.
   */
  return ((unsigned) val >= 0xffff0000u);
}

extern int NaClXlateErrno(int errnum);

extern int NaClXlateNaClSyncStatus(NaClSyncStatus status);

/*
 * Mapping data from a file.
 *
 * start_addr and len must be multiples of NACL_MAP_PAGESIZE.
 *
 * Of prot bits, only PROT_READ and PROT_WRITE are allowed.  Of flags,
 * only MAP_ANONYMOUS is allowed.  start_addr must be specified, and
 * this code will add in MAP_FIXED.  start_address, len, and offset
 * must be a multiple of NACL_MAP_PAGESIZE.
 *
 * Note that in Windows, in order for the mapping to be coherent, the
 * mapping must arise from the same mapping handle and just using the
 * same file handle won't ensure coherence.  If the file mapping
 * object were created and destroyed inside of NaClHostDescMap, there
 * would never be any chance at coherence.  One alternative is to
 * create a file mapping object for each mapping mode.  Native
 * descriptors are then shareable, but only when the mode matches (!).
 * A read-only shared mapping not seeing the changes made by a
 * read-write mapping seem rather ugly.
 *
 * Instead of this ugliness, we just say that a map operation cannot
 * request MAP_SHARED.  Anonymous mappings ignore the descriptor
 * argument.
 *
 * Underlying host-OS syscalls:  mmap / MapViewOfFileEx
 *
 */
extern int NaClHostDescMap(struct NaClHostDesc  *d,
                           void                 *start_addr,
                           size_t               len,
                           int                  prot,
                           int                  flags,
                           off_t                offset);  /* 4GB file max */

/*
 * Undo a file mapping.  The memory range specified by start_address,
 * len must be memory that came from NaClHostDescMap.
 *
 * start_addr and len must be multiples of NACL_MAP_PAGESIZE.
 *
 * Underlying host-OS syscalls: mmap / UnmapViewOfFile/VirtualAlloc
 */
extern int NaClHostDescUnmap(void   *start_addr,
                             size_t len);

/*
 * Undo a file mapping.  The memory range specified by start_address,
 * len must be memory that came from NaClHostDescMap.
 *
 * start_addr and len must be multiples of NACL_MAP_PAGESIZE.
 *
 * Underlying host-OS syscalls: munmap / UnmapViewOfFile
 */
extern int NaClHostDescUnmapUnsafe(void   *start_addr,
                                   size_t len);

/*
 * Constructor for a NaClHostDesc object.
 *
 * path should be a host-OS pathname to a file.  No validation is
 * done.  mode should contain one of O_RDONLY, O_WRONLY, and O_RDWR,
 * and can additionally contain O_CREAT, O_TRUNC, and O_APPEND.
 *
 * Uses raw syscall return convention, so returns 0 for success and
 * non-zero (usually -NACL_ABI_EINVAL) for failure.
 *
 * We cannot return the platform descriptor/handle and be consistent
 * with a largely POSIX-ish interface, since e.g. windows handles may
 * be negative and might look like negative errno returns.  Currently
 * we use the posix API on windows, so it could work, but we may need
 * to change later.
 *
 * Underlying host-OS functions: open / _s_open_s
 */
extern int NaClHostDescOpen(struct NaClHostDesc *d,
                            char                *path,
                            int                 mode,
                            int                 perms);

/*
 * Constructor for a NaClHostDesc object.
 *
 * Uses raw syscall return convention, so returns 0 for success and
 * non-zero (usually -NACL_ABI_EINVAL) for failure.
 *
 * d is a POSIX-interface descriptor
 *
 * mode may only contain one of NACL_ABI_O_RDONLY, NACL_ABI_O_WRONLY,
 * or NACL_ABI_O_RDWR, and must be the actual mode that d was opened
 * with.  Note that these are host OS access mode bits.
 *
 * Underlying host-OS functions: dup / _dup; mode is what posix_d was
 * opened with
 */
extern int NaClHostDescPosixDup(struct NaClHostDesc *d,
                                int                 posix_d,
                                int                 mode);

/*
 * Essentially the same as NaClHostDescPosixDup, but without the dup
 * -- takes ownership of the descriptor rather than making a dup.
 */
extern int NaClHostDescPosixTake(struct NaClHostDesc  *d,
                                 int                  posix_d,
                                 int                  mode);


/*
 * Allocates a NaClHostDesc and invokes NaClHostDescPosixTake on it.
 * Aborts process if no memory.
 */
extern struct NaClHostDesc *NaClHostDescPosixMake(int posix_d,
                                                  int mode);
/*
 * Read data from an opened file into a memory buffer.
 *
 * buf is not validated.
 *
 * Underlying host-OS functions: read / _read
 */
extern ssize_t NaClHostDescRead(struct NaClHostDesc *d,
                                void                *buf,
                                size_t              len);


/*
 * Write data from a memory buffer into an opened file.
 *
 * buf is not validated.
 *
 * Underlying host-OS functions: write / _write
 */
extern ssize_t NaClHostDescWrite(struct NaClHostDesc  *d,
                                 void const           *buf,
                                 size_t               count);

extern int NaClHostDescSeek(struct NaClHostDesc *d,
                            off_t               offset,
                            int                 whence);

/*
 * TODO(bsy): Need to enumerate which request is supported and the
 * size of the argument, as well as whether the arg is input or
 * output.
 */
extern int NaClHostDescIoctl(struct NaClHostDesc  *d,
                             int                  request,
                             void                 *arg);

/*
 * Fstat.
 */
extern int NaClHostDescFstat(struct NaClHostDesc  *d,
                             struct nacl_abi_stat *nasp);

/*
 * Dtor for the NaClHostFile object. Close the file.
 *
 * Underlying host-OS functions:  close(2) / _close
 */
extern int NaClHostDescClose(struct NaClHostDesc  *d);

extern int NaClHostDescStat(char const            *host_os_pathname,
                            struct nacl_abi_stat  *nasp);

/*
 * should DCE away when unused.
 * TODO: explain what this is good for
 */

extern int NaClProtMap(int abi_prot);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_HOST_DESC_H__ */
