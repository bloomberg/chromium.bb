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
 * NaCl Service Runtime memory allocation code
 */
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/include/nacl_platform.h"


void NaCl_page_free(void     *p,
                    size_t   size) {
  if (p == 0 || size == 0)
    return;
  (void) munmap(p, size);
}


/*
 * Note that NaCl_page_alloc does not allocate pages that satisify
 * NaClIsAllocPageMultiple.  On linux/osx, the system does not impose
 * any such restrictions, and we only need to enforce the restriction
 * on NaCl app code to ensure that the app code is portable across all
 * host OSes.
 */

int NaCl_page_alloc_intern(void   **p,
                           size_t size) {
  void *addr;
  int map_flags = MAP_PRIVATE | MAP_ANONYMOUS;

  if (NULL != *p) {
    map_flags |= MAP_FIXED;
  }
  addr = mmap(*p,
              size,
              PROT_EXEC | PROT_READ | PROT_WRITE,
              map_flags,
              -1,
              (off_t) 0);
  if (MAP_FAILED == addr) {
    addr = NULL;
  }
  if (NULL != addr) {
    *p = addr;
  }
  return (NULL == addr) ? -ENOMEM : 0;
}

int NaCl_page_alloc(void   **p,
                    size_t size) {
  void *addr = NULL;
  int rv;

  if (0 == (rv = NaCl_page_alloc_intern(&addr, size))) {
    *p = addr;
  }

  return rv;
}

int NaCl_page_alloc_at_addr(void   **p,
                            size_t size) {
  return NaCl_page_alloc_intern(p, size);
}

/*
* This is critical to make the text region non-writable, and the data
* region read/write but no exec.  Of course, some kernels do not
* respect the lack of PROT_EXEC.
*/
int NaCl_mprotect(void          *addr,
                  size_t        len,
                  int           prot) {
  int  ret = mprotect(addr, len, prot);

  return ret == -1 ? -errno : ret;
}


int NaCl_madvise(void           *start,
                 size_t         length,
                 int            advice) {
  int ret = madvise(start, length, advice);

  /*
   * MADV_DONTNEED and MADV_NORMAL are needed
   */
  return ret == -1 ? -errno : ret;
}
