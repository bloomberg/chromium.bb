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
 * NaCl Service Runtime.  Directory descriptor / Handle abstraction.
 */
#include "native_client/src/include/portability.h"
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <share.h>

#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_host_dir.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/win/xlate_system_error.h"

#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"
#include "native_client/src/trusted/service_runtime/internal_errno.h"

#include "native_client/src/trusted/service_runtime/include/sys/dirent.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/sys/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"


int NaClHostDirOpen(struct NaClHostDir  *d,
                    char                *path) {
  char    pattern[NACL_CONFIG_PATH_MAX];

  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDirOpen: 'this' is NULL\n");
  }

  _snprintf(pattern, sizeof(pattern), "%s\\*.*", path);
  d->handle = FindFirstFile(pattern, &d->find_data);
  d->off = 0;
  d->done = 0;

  if (INVALID_HANDLE_VALUE == d->handle) {
    int retval = GetLastError();
    NaClLog(LOG_ERROR, "NaClHostDirOpen: failed\n");
    if (retval == ERROR_NO_MORE_FILES) {
      d->done = 1;
      return 0;
    } else {
      /* TODO(sehr): fix the errno handling */
      return -NaClXlateSystemError(retval);
    }
  }

  return 0;
}

ssize_t NaClHostDirGetdents(struct NaClHostDir  *d,
                            void                *buf,
                            size_t               len) {
  struct nacl_abi_dirent *p;
  unsigned int           i;

  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDirGetdents: 'this' is NULL\n");
  }
  NaClLog(3, "NaClHostDirGetdents(0x%08x, %u):\n", buf, len);

  p = (struct nacl_abi_dirent *) buf;
  i = 0;
  d->off = 1;
  while (1) {
    int name_length = strlen(d->find_data.cFileName) + 1;
    int rec_length = (offsetof(struct nacl_abi_dirent, nacl_abi_d_name) +
                      (name_length + 3)) & ~3;
    if (d->done || (i + rec_length >= len)) {
      return i;
    }

    p = (struct nacl_abi_dirent *) (((char *) buf) + i);
    p->nacl_abi_d_ino = 0x6c43614e;
    p->nacl_abi_d_off = d->off;
    p->nacl_abi_d_reclen = rec_length;
    memcpy(p->nacl_abi_d_name, d->find_data.cFileName, name_length);
    i += p->nacl_abi_d_reclen;
    ++d->off;

    if (!FindNextFile(d->handle, &d->find_data)) {
      int retval = GetLastError();
      if (retval == ERROR_NO_MORE_FILES) {
        d->done = 1;
        return i;
      } else {
        return -NaClXlateSystemError(retval);
      }
    }
  }
}

int NaClHostDirClose(struct NaClHostDir *d) {
  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDirClose: 'this' is NULL\n");
  }
  if (!FindClose(d->handle)) {
    return -NaClXlateSystemError(GetLastError());
  }
  return 0;
}
