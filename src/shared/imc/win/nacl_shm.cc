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


// NaCl inter-module communication primitives.

#include <windows.h>
#include "native_client/src/shared/imc/nacl_imc.h"

namespace nacl {

Handle CreateMemoryObject(size_t length) {
  if (length % kMapPageSize) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return kInvalidHandle;
  }
  Handle memory = CreateFileMapping(
      INVALID_HANDLE_VALUE,
      NULL,
      PAGE_READWRITE,
      static_cast<DWORD>(static_cast<unsigned __int64>(length) >> 32),
      static_cast<DWORD>(length & 0xFFFFFFFF), NULL);
  return (memory == NULL) ? kInvalidHandle : memory;
}

void* Map(void* start, size_t length, int prot, int flags,
          Handle memory, off_t offset) {
  static DWORD prot_to_access[4] = {
    FILE_MAP_READ,  // TBD
    FILE_MAP_READ,
    FILE_MAP_WRITE,
    FILE_MAP_ALL_ACCESS
  };

  if (!(flags & (kMapShared | kMapPrivate))) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return kMapFailed;
  }

  // Convert prot to the desired access type for MapViewOfFileEx().
  DWORD desired_access = prot_to_access[prot & 0x3];
  if (flags & kMapPrivate) {
    desired_access = FILE_MAP_COPY;
  }

  start = MapViewOfFileEx(memory, desired_access, 0, offset, length,
                          (flags & kMapFixed) ? start : 0);
  return start ? start : kMapFailed;
}

int Unmap(void* start, size_t length) {
  // TODO(shiki): Try from start to start + length
  UnmapViewOfFile(start);
  return 0;
}

}  // namespace nacl
