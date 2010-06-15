/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// NaCl inter-module communication primitives.

#include <windows.h>
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/platform/nacl_log.h"

namespace nacl {

Handle CreateMemoryObject(size_t length) {
  if (length % kMapPageSize) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return kInvalidHandle;
  }
  Handle memory = CreateFileMapping(
      INVALID_HANDLE_VALUE,
      NULL,
      PAGE_EXECUTE_READWRITE,
      static_cast<DWORD>(static_cast<unsigned __int64>(length) >> 32),
      static_cast<DWORD>(length & 0xFFFFFFFF), NULL);
  return (memory == NULL) ? kInvalidHandle : memory;
}

void* Map(void* start, size_t length, int prot, int flags,
          Handle memory, off_t offset) {
  static DWORD prot_to_access[] = {
    FILE_MAP_READ,  // TBD
    FILE_MAP_READ,
    FILE_MAP_WRITE,
    FILE_MAP_ALL_ACCESS,
    FILE_MAP_EXECUTE,
    FILE_MAP_READ | FILE_MAP_EXECUTE,
    FILE_MAP_WRITE | FILE_MAP_EXECUTE,
    FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE
  };

  if (!(flags & (kMapShared | kMapPrivate))) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return kMapFailed;
  }

  // Convert prot to the desired access type for MapViewOfFileEx().
  DWORD desired_access = prot_to_access[prot & 0x7];
  if (flags & kMapPrivate) {
    desired_access = FILE_MAP_COPY;
  }

  start = MapViewOfFileEx(memory, desired_access, 0, offset, length,
                          (flags & kMapFixed) ? start : 0);
  return (NULL != start) ? start : kMapFailed;
}

int Unmap(void* start, size_t length) {
  // TODO(shiki): Try from start to start + length
  UnmapViewOfFile(start);
  return 0;
}

}  // namespace nacl
