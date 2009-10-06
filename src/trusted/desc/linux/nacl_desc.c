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

#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"

void NaClDeallocAddrRange(uintptr_t addr,
                          size_t    len) {
  UNREFERENCED_PARAMETER(addr);
  UNREFERENCED_PARAMETER(len);
  return;
}

/*
 * Not quite a copy ctor.  Call it a translating ctor, since the
 * struct nacl_abi_stat POD object is constructed from the
 * nacl_host_stat_t POD object by element-wise copying.
 */
int32_t NaClAbiStatHostDescStatXlateCtor(struct nacl_abi_stat    *dst,
                                         nacl_host_stat_t const  *src) {
  nacl_abi_mode_t m;

  /*
   * should the caller be using nacl_abi_stat64 if/when that's available?
   */
  if (src->st_size > INT32_MAX) {
    return -NACL_ABI_EOVERFLOW;
  }
  memset(dst, 0, sizeof *dst);

  dst->nacl_abi_st_dev = 0;
  dst->nacl_abi_st_ino = 0x6c43614e;

  switch (src->st_mode & S_IFMT) {
    case S_IFREG:
      m = NACL_ABI_S_IFREG;
      break;
    case S_IFDIR:
      m = NACL_ABI_S_IFDIR;
      break;
#if defined(S_IFCHR)
    case S_IFCHR:
      /* stdin/out/err can be inherited, so this is okay */
      m = NACL_ABI_S_IFCHR;
      break;
#endif
    default:
      NaClLog(LOG_ERROR,
              ("NaClAbiStatHostDescStatXlateCtor: how did NaCl app open a file"
               " with st_mode = 0%o?\n"),
              src->st_mode);
      m = NACL_ABI_S_UNSUP;
  }
  if (0 != (dst->nacl_abi_st_mode & S_IRUSR)) {
      m |= NACL_ABI_S_IRUSR;
  }
  if (0 != (dst->nacl_abi_st_mode & S_IWUSR)) {
      m |= NACL_ABI_S_IWUSR;
  }
  if (0 != (dst->nacl_abi_st_mode & S_IXUSR)) {
      m |= NACL_ABI_S_IXUSR;
  }
  dst->nacl_abi_st_mode = m;
  dst->nacl_abi_st_nlink = src->st_nlink;
  dst->nacl_abi_st_uid = -1;  /* not root */
  dst->nacl_abi_st_gid = -1;  /* not wheel */
  dst->nacl_abi_st_rdev = 0;
  dst->nacl_abi_st_size = (nacl_abi_off_t) src->st_size;
  dst->nacl_abi_st_blksize = 0;
  dst->nacl_abi_st_blocks = 0;
  dst->nacl_abi_st_atime = src->st_atime;
  dst->nacl_abi_st_mtime = src->st_mtime;
  dst->nacl_abi_st_ctime = src->st_ctime;

  return 0;
}
