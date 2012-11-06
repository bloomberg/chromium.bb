/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <windows.h>

#include "native_client/src/shared/platform/nacl_find_addrsp.h"

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_log.h"

/* bool */
int NaClFindAddressSpace(uintptr_t *addr, size_t memory_size) {
  void *map_addr;

  NaClLog(4,
          "NaClFindAddressSpace: looking for %"NACL_PRIxS" bytes\n",
          memory_size);
  map_addr = VirtualAlloc(NULL, memory_size, MEM_RESERVE, PAGE_READWRITE);
  if (NULL == map_addr) {
    NaClLog(LOG_ERROR,
            ("NaClFindAddressSpace: VirtualAlloc failed looking for"
             " 0x%"NACL_PRIxS" bytes\n"),
            memory_size);
    return 0;
  }
  if (!VirtualFree(map_addr, 0, MEM_RELEASE)) {
    NaClLog(LOG_FATAL,
            ("NaClFindAddressSpace: VirtualFree of VirtualAlloc result"
             " (0x%"NACL_PRIxPTR"failed, GetLastError %d.\n"),
            (uintptr_t) map_addr,
            GetLastError());
  }
  NaClLog(4,
          "NaClFindAddressSpace: got addr %"NACL_PRIxPTR"\n",
          (uintptr_t) map_addr);
  *addr = (uintptr_t) map_addr;
  return 1;
}
