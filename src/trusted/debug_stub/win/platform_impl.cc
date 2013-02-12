/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include <exception>

#include "native_client/src/include/nacl_compiler_annotations.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/debug_stub/abi.h"
#include "native_client/src/trusted/debug_stub/platform.h"

/*
 * Define the OS specific portions of IPlatform interface.
 */

static DWORD Reprotect(void *ptr, uint32_t len, DWORD newflags) {
  DWORD oldflags;
  MEMORY_BASIC_INFORMATION memory_info;
  SIZE_T offset;
  SIZE_T max_step;
  char *current_pointer = reinterpret_cast<char*>(ptr);
  SIZE_T current_len = len;

  // Check that all memory is accessible.
  while (current_len > 0) {
    if (!VirtualQuery(current_pointer, &memory_info, sizeof(memory_info))) {
      NaClLog(LOG_ERROR, "Reprotect: VirtualQuery failed with %d\n",
              GetLastError());
      return -1;
    }
    if (memory_info.Protect == PAGE_NOACCESS) {
      return -1;
    }
    offset = current_pointer - reinterpret_cast<char*>(memory_info.BaseAddress);
    max_step = memory_info.RegionSize - offset;
    if (current_len <= max_step) {
      break;
    }
    current_len -= max_step;
    current_pointer = current_pointer + max_step;
  }
  if (!VirtualProtect(ptr, len, newflags, &oldflags)) {
    NaClLog(LOG_ERROR, "Reprotect: VirtualProtect failed with %d\n",
            GetLastError());
    return -1;
  }

  FlushInstructionCache(GetCurrentProcess(), ptr, len);
  return oldflags;
}

namespace port {

bool IPlatform::GetMemory(uint64_t virt, uint32_t len, void *dst) {
  uint32_t oldFlags = Reprotect(reinterpret_cast<void*>(virt),
                                len, PAGE_EXECUTE_READWRITE);

  if (oldFlags == -1) return false;

  memcpy(dst, reinterpret_cast<void*>(virt), len);
  (void) Reprotect(reinterpret_cast<void*>(virt), len, oldFlags);
  return true;
}

bool IPlatform::SetMemory(struct NaClApp *nap, uint64_t virt, uint32_t len,
                          void *src) {
  UNREFERENCED_PARAMETER(nap);
  uint32_t oldFlags = Reprotect(reinterpret_cast<void*>(virt),
                                len, PAGE_EXECUTE_READWRITE);

  if (oldFlags == -1) return false;

  memcpy(reinterpret_cast<void*>(virt), src, len);
  FlushInstructionCache(GetCurrentProcess(),
                        reinterpret_cast<void*>(virt), len);
  (void) Reprotect(reinterpret_cast<void*>(virt), len, oldFlags);
  return true;
}

}  // End of port namespace

