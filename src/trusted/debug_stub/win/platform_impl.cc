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

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/debug_stub/abi.h"
#include "native_client/src/trusted/debug_stub/platform.h"

/*
 * Define the OS specific portions of IPlatform interface.
 */

static DWORD Reprotect(void *ptr, uint32_t len, DWORD newflags) {
  DWORD oldflags;
  if (!VirtualProtect(ptr, len, newflags, &oldflags)) {
    printf("Failed with %d\n", GetLastError());
    return -1;
  }

  FlushInstructionCache(GetCurrentProcess(), ptr, len);
  return oldflags;
}

namespace port {

void IPlatform::Relinquish(uint32_t msec) {
  Sleep(msec);
}

bool IPlatform::GetMemory(uint64_t virt, uint32_t len, void *dst) {
  uint32_t oldFlags = Reprotect(reinterpret_cast<void*>(virt),
                                len, PAGE_EXECUTE_READWRITE);

  if (oldFlags == -1) return false;

  memcpy(dst, reinterpret_cast<void*>(virt), len);
  (void) Reprotect(reinterpret_cast<void*>(virt), len, oldFlags);
  return true;
}

bool IPlatform::SetMemory(uint64_t virt, uint32_t len, void *src) {
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

