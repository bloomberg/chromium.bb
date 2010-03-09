/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Service Runtime.  Directory descriptor / Handle abstraction.
 *
 * Note that we avoid using the thread-specific data / thread local
 * storage access to the "errno" variable, and instead use the raw
 * system call return interface of small negative numbers as errors.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>

#include "native_client/src/include/nacl_platform.h"

#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_host_dir.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"

#include "native_client/src/trusted/service_runtime/include/sys/dirent.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/sys/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"


int NaClHostDirOpen(struct NaClHostDir  *d,
                    char                *path) {
  DIR  *dirp;

  NaClLog(3, "NaClHostDirOpen(0x%08"NACL_PRIxPTR", %s)\n", (uintptr_t) d, path);
  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDirOpen: 'this' is NULL\n");
  }

  NaClLog(3, "NaClHostDirOpen: invoking POSIX opendir(%s)\n", path);
  dirp = opendir(path);
  NaClLog(3, "NaClHostDirOpen: got DIR* 0x%08"NACL_PRIxPTR"\n",
          (uintptr_t) dirp);
  if (NULL == dirp) {
    NaClLog(LOG_ERROR,
            "NaClHostDirOpen: open returned NULL, errno %d\n", errno);
    return -NaClXlateErrno(errno);
  }
  d->dirp = dirp;
  d->dp = readdir(d->dirp);
  d->off = 0;
  NaClLog(3, "NaClHostDirOpen: success.\n");
  return 0;
}

ssize_t NaClHostDirGetdents(struct NaClHostDir  *d,
                            void                *buf,
                            size_t              len) {
  struct nacl_abi_dirent *p;
  int                    rec_length;
  size_t                 i;

  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDirGetdents: 'this' is NULL\n");
  }
  NaClLog(3, "NaClHostDirGetdents(0x%08"NACL_PRIxPTR", %"NACL_PRIuS"):\n",
          (uintptr_t) buf, len);

  i = 0;
  while (i < len) {
    if ((NULL == d->dp) || (i + d->dp->d_reclen > len)) {
      return i;
    }
    p = (struct nacl_abi_dirent *) (((char *) buf) + i);
    p->nacl_abi_d_ino = 0x6c43614e;
    p->nacl_abi_d_off = ++d->off;
    memcpy(p->nacl_abi_d_name,
           d->dp->d_name,
           d->dp->d_namlen + 1);
    /*
     * Newlib expects entries to start on 0mod4 boundaries.
     * Round reclen to the next multiple of four.
     */
    rec_length = (offsetof(struct nacl_abi_dirent, nacl_abi_d_name) +
                  (d->dp->d_namlen + 4)) & ~3;
    /*
     * We cast to a volatile pointer so that the compiler won't ever
     * pick up the rec_length value from that user-accessible memory
     * location, rather than actually using the value in a register or
     * in the local frame.
     */
    ((volatile struct nacl_abi_dirent *) p)->nacl_abi_d_reclen = rec_length;
    i += rec_length;
    d->dp = readdir(d->dirp);
  }

  return i;
}

int NaClHostDirClose(struct NaClHostDir *d) {
  int retval;

  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDirClose: 'this' is NULL\n");
  }
  NaClLog(3, "NaClHostDirClose(0x%08"NACL_PRIxPTR")\n", (uintptr_t) d->dirp);
  retval = closedir(d->dirp);
  if (-1 != retval) {
    d->dirp = NULL;
  }
  return (-1 == retval) ? -NaClXlateErrno(errno) : retval;
}
