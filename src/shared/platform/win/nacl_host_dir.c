/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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
  wchar_t    pattern[NACL_CONFIG_PATH_MAX + 1];
  int err;

  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDirOpen: 'this' is NULL\n");
  }


  /**
    * "path" is an 8-bit char string. Convert to UTF-16 here.
    * Using Microsoft "Secure CRT" snprintf. Passing _TRUNCATE
    * instructs the runtime to truncate and return -1 if the
    * buffer is too small, rather than the default behavior of
    * dumping core.
    *
    * NOTE: %hs specifies a single-byte-char string.
    */
  err = _snwprintf_s(pattern,
                     NACL_CONFIG_PATH_MAX + 1,
                     _TRUNCATE, L"%hs\\*.*", path);
  if (err < 0) {
    return -NACL_ABI_EOVERFLOW;
  }
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
  size_t                 i;

  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDirGetdents: 'this' is NULL\n");
  }
  NaClLog(3, "NaClHostDirGetdents(0x%08x, %u):\n", buf, len);

  p = (struct nacl_abi_dirent *) buf;
  i = 0;
  d->off = 1;
  while (1) {
    /**
     * The FIND_DATA structure contains the filename as a UTF-16
     * string of length MAX_PATH. This may or may not convert into
     * an 8-bit char string of similar length. The safe thing to do here
     * is to assume the worst case: every UTF-16 character expands
     * to a four-byte UTF8 sequence.
     *
     * NB: Keep in mind that MAX_PATH is an API limitation, not a
     * limitation of the underlying filesystem.
     */
    char name_utf8[(MAX_PATH * 4) + 1];
    size_t name_length;
    size_t rec_length;
    uint16_t nacl_abi_rec_length;
    int err;

    err = _snprintf_s(name_utf8,
                      _countof(name_utf8),
                      _TRUNCATE,
                      "%ws", d->find_data.cFileName);
    if (err < 0) {
      return -NACL_ABI_EOVERFLOW;
    }
    name_length = strlen(name_utf8) + 1;
    rec_length = (offsetof(struct nacl_abi_dirent, nacl_abi_d_name)
                    + (name_length + 3))
                  & ~3;

    /* Check for overflow in record length */
    nacl_abi_rec_length = (uint16_t)rec_length;
    if (rec_length > (size_t)nacl_abi_rec_length) {
      return -NACL_ABI_EOVERFLOW;
    }

    if (d->done || (i + rec_length >= len)) {
      return i;
    }

    p = (struct nacl_abi_dirent *) (((char *) buf) + i);
    p->nacl_abi_d_ino = 0x6c43614e;
    p->nacl_abi_d_off = d->off;
    p->nacl_abi_d_reclen = nacl_abi_rec_length;
    memcpy(p->nacl_abi_d_name, name_utf8, name_length);
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
