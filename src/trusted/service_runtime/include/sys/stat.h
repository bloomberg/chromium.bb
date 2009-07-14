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
 * NaCl kernel / service run-time system call ABI.  stat/fstat.
 */

#ifndef SERVICE_RUNTIME_INCLUDE_SYS_STAT_H_
#define SERVICE_RUNTIME_INCLUDE_SYS_STAT_H_

#ifdef __native_client__
#include <sys/types.h>
#else
#include "native_client/src/trusted/service_runtime/include/machine/_types.h"
#endif
#include "native_client/src/trusted/service_runtime/include/bits/stat.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Linux <bit/stat.h> uses preprocessor to define st_atime to
 * st_atim.tv_sec etc to widen the ABI to use a struct timespec rather
 * than just have a time_t access/modification/inode-change times.
 * this is unfortunate, since that symbol cannot be used as a struct
 * member elsewhere (!).
 *
 * just like with type name conflicts, we avoid it by using nacl_abi_
 * as a prefix for struct members too.  sigh.
 */

struct nacl_abi_stat {  /* must be renamed when ABI is exported */
  nacl_abi_dev_t     nacl_abi_st_dev;      /* not implemented */
  nacl_abi_ino_t     nacl_abi_st_ino;      /* not implemented */
  nacl_abi_mode_t    nacl_abi_st_mode;     /* partially implemented. */
  nacl_abi_nlink_t   nacl_abi_st_nlink;    /* link count */
  nacl_abi_uid_t     nacl_abi_st_uid;      /* not implemented */
  nacl_abi_gid_t     nacl_abi_st_gid;      /* not implemented */
  int                __padding;            /* needed to align st_rdev */
  nacl_abi_dev_t     nacl_abi_st_rdev;     /* not implemented */
  nacl_abi_off_t     nacl_abi_st_size;     /* object size */
  nacl_abi_blksize_t nacl_abi_st_blksize;  /* not implemented */
  nacl_abi_blkcnt_t  nacl_abi_st_blocks;   /* not implemented */
  nacl_abi_time_t    nacl_abi_st_atime;    /* access time */
  nacl_abi_time_t    nacl_abi_st_mtime;    /* modification time */
  nacl_abi_time_t    nacl_abi_st_ctime;    /* inode change time */
};

#ifdef __native_client__
int stat(char const *path, struct nacl_abi_stat *stbuf);
int fstat(int d, struct nacl_abi_stat *stbuf);
#endif

#ifdef __cplusplus
}
#endif

#endif
