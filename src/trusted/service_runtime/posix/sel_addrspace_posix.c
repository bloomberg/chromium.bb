/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <sys/resource.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/sel_addrspace.h"

/*
 * When we're built into Chromium's "nacl_helper", its main will set this.
 */
void *g_nacl_prereserved_sandbox_addr = NULL;

/*
 * Find sandbox memory pre-reserved by the nacl_helper in chrome. The
 * nacl_helper, if present, reserves the bottom 1G of the address space
 * for use by Native Client.
 *
 * NOTE: num_bytes is currently ignored. It should be 1GB on Linux and
 * 1GB plus a few pages on ARM. TODO(bradchen): deal with num_bytes.
 *
 * Out parameter p should be either:
 *   0: reserved memory was not found
 *   less than 128K: indicates the bottom 1G was reserved.
 */
int NaClFindPrereservedSandboxMemory(void **p, size_t num_bytes) {
  UNREFERENCED_PARAMETER(num_bytes);

  NaClLog(2,
          "NaClFindPrereservedSandboxMemory(, %#.8"NACL_PRIxPTR") => %p\n",
          num_bytes, g_nacl_prereserved_sandbox_addr);

  *p = g_nacl_prereserved_sandbox_addr;
  return g_nacl_prereserved_sandbox_addr != NULL;
}

/*
 * If we have a soft limit for RLIMIT_AS lower than the hard limit,
 * increase it to include enough for the untrusted address space.
 */
void NaClAddrSpaceBeforeAlloc(size_t untrusted_size) {
  struct rlimit lim;

  UNREFERENCED_PARAMETER(untrusted_size);

  if (getrlimit(RLIMIT_AS, &lim) < 0) {
    NaClLog(LOG_WARNING, "NaClAddrSpaceBeforeAlloc: getrlimit failed: %s",
            strerror(errno));
    return;
  }

  /*
   * We don't just increase it by untrusted_size, because we need a
   * contiguous piece of that size, and that may stretch the overall
   * address space we need to occupy well beyond the sum of the old
   * limit and untrusted_size.
   */
  if (lim.rlim_cur < lim.rlim_max) {
    NaClLog(LOG_INFO, "NaClAddrSpaceBeforeAlloc: "
            "raising RLIMIT_AS from %#lx to hard limit %#lx\n",
            (unsigned long) lim.rlim_cur,
            (unsigned long) lim.rlim_max);
    lim.rlim_cur = lim.rlim_max;
    if (setrlimit(RLIMIT_AS, &lim) < 0) {
      NaClLog(LOG_WARNING, "NaClAddrSpaceBeforeAlloc: setrlimit failed: %s",
              strerror(errno));
    }
  }
}
