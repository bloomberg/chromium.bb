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
 *
 * Note that we avoid using the thread-specific data / thread local
 * storage access to the "errno" variable, and instead use the raw
 * system call return interface of small negative numbers as errors.
 */

#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/types.h>
#include <linux/unistd.h>

#include "native_client/src/include/nacl_platform.h"

#include "native_client/src/trusted/platform/nacl_host_desc.h"
#include "native_client/src/trusted/platform/nacl_host_dir.h"
#include "native_client/src/trusted/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"

#include "native_client/src/trusted/service_runtime/include/sys/dirent.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/sys/mman.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"

#ifdef _syscall3
_syscall3(int, getdents, uint, fd, struct dirent *, dirp, uint, count)

int getdents(unsigned int fd, struct dirent* dirp, unsigned int count);
#else
# include <sys/syscall.h>
int getdents(unsigned int fd, struct dirent* dirp, unsigned int count) {
  return syscall(SYS_getdents, fd, dirp, count);
}
#endif

int NaClHostDirOpen(struct NaClHostDir  *d,
                    char                *path) {
  int  fd;

  NaClLog(3, "NaClHostDirOpen(0x%08"PRIxPTR", %s)\n", (uintptr_t) d, path);
  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDirOpen: 'this' is NULL\n");
  }

  NaClLog(3, "NaClHostDirOpen: invoking open(%s)\n", path);
  fd = open(path, O_RDONLY);
  NaClLog(3, "NaClHostDirOpen: got DIR* %d\n", fd);
  if (-1 == fd) {
    NaClLog(LOG_ERROR,
            "NaClHostDirOpen: open returned -1, errno %d\n", errno);
    return -NaClXlateErrno(errno);
  }
  d->fd = fd;
  NaClLog(3, "NaClHostDirOpen: success.\n");
  return 0;
}

ssize_t NaClHostDirGetdents(struct NaClHostDir  *d,
                            void                *buf,
                            size_t              len) {
  struct dirent           *host_direntp = (struct dirent *) buf;
  struct nacl_abi_dirent  *p;
  int                     retval;
  int                     i;

  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDirGetdents: 'this' is NULL\n");
  }
  NaClLog(3, "NaClHostDirGetdents(0x%08"PRIxPTR", %"PRIuS"):\n",
          (uintptr_t) buf, len);
  retval = getdents(d->fd, host_direntp, len);
  if (-1 == retval) {
    return -NaClXlateErrno(errno);
  } else if (0 == retval) {
    return 0;
  }

  NaClLog(3, "NaClHostDirGetdents: returned %d\n", retval);

  /*
   * Scrub inode numbers.
   */
  i = 0;
  while (i < retval) {
    p = (struct nacl_abi_dirent *) (((char *) buf) + i);
    /*
     * Make sure we're not tricked into writing outside of the buffer,
     * because.... (see below)
     */
    if ((((char *) &p->nacl_abi_d_ino) + sizeof(p->nacl_abi_d_ino)
         - (char *) buf) <= retval) {
      p->nacl_abi_d_ino = 0x6c43614e;
    }
    /*
     * ... we may pick up a user controlled/bogus value by following p
     * here, so i could make p point to the last byte of the buffer,
     * and p->* would be beyond the buffer.
     */
    i += p->nacl_abi_d_reclen;
  }

  return retval;
}

int NaClHostDirClose(struct NaClHostDir *d) {
  int retval;

  if (NULL == d) {
    NaClLog(LOG_FATAL, "NaClHostDirClose: 'this' is NULL\n");
  }
  NaClLog(3, "NaClHostDirClose(%d)\n", d->fd);
  retval = close(d->fd);
  d->fd = -1;
  return (-1 == retval) ? -NaClXlateErrno(errno) : retval;
}
