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

#ifndef NACL_ABI__SYS_DIRENT_H
#define NACL_ABI__SYS_DIRENT_H

#ifdef __native_client__
#include "native_client/src/trusted/service_runtime/include/sys/types.h"
#else
#include "native_client/src/trusted/service_runtime/include/machine/_types.h"
#endif

/*
 * We need a way to define the maximum size of a name.
 */
#ifndef MAXNAMLEN
# ifdef NAME_MAX
#  define MAXNAMLEN NAME_MAX
# else
#  define MAXNAMLEN 255
# endif
#endif

/*
 * _dirdesc contains the state used by opendir/readdir/closedir.
 */
typedef struct nacl_abi__dirdesc {
  int   nacl_abi_dd_fd;
  long  nacl_abi_dd_loc;
  long  nacl_abi_dd_size;
  char  *nacl_abi_dd_buf;
  int   nacl_abi_dd_len;
  long  nacl_abi_dd_seek;
} nacl_abi_DIR;

/*
 * dirent represents a single directory entry.
 */
struct nacl_abi_dirent {
  nacl_abi_ino_t nacl_abi_d_ino;
  nacl_abi_off_t nacl_abi_d_off;
  uint16_t       nacl_abi_d_reclen;
  char           nacl_abi_d_name[MAXNAMLEN + 1];
};

/*
 * external function declarations
 */
extern nacl_abi_DIR           *nacl_abi_opendir(const char *dirpath);
extern struct nacl_abi_dirent *nacl_abi_readdir(nacl_abi_DIR *direntry);
extern int                    nacl_abi_closedir(nacl_abi_DIR *direntry);

#endif
