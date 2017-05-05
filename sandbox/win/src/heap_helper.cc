// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/heap_helper.h"

#include <windows.h>

#include "base/memory/ref_counted.h"
#include "base/win/windows_version.h"

namespace sandbox {

// These are undocumented, but readily found on the internet.
#define HEAP_CLASS_8 0x00008000  // CSR port heap
#define HEAP_CLASS_MASK 0x0000f000

// This structure is not documented, but the flags field is the only relevant
// field.
struct _HEAP {
  char reserved[0x70];
  DWORD flags;
};

bool HeapFlags(HANDLE handle, DWORD* flags) {
  if (!handle || !flags) {
    // This is an error.
    return false;
  }
  _HEAP* heap = reinterpret_cast<_HEAP*>(handle);
  *flags = heap->flags;
  return true;
}

HANDLE FindCsrPortHeap() {
  if (base::win::GetVersion() < base::win::VERSION_WIN10) {
    // This functionality has not been verified on versions before Win10.
    return nullptr;
  }
  DWORD number_of_heaps = ::GetProcessHeaps(0, NULL);
  std::unique_ptr<HANDLE[]> all_heaps(new HANDLE[number_of_heaps]);
  if (::GetProcessHeaps(number_of_heaps, all_heaps.get()) != number_of_heaps)
    return nullptr;

  // Search for the CSR port heap handle, identified purely based on flags.
  HANDLE csr_port_heap = nullptr;
  for (size_t i = 0; i < number_of_heaps; ++i) {
    HANDLE handle = all_heaps[i];
    DWORD flags = 0;
    if (!HeapFlags(handle, &flags)) {
      DLOG(ERROR) << "Unable to get flags for this heap";
      continue;
    }
    if ((flags & HEAP_CLASS_MASK) == HEAP_CLASS_8) {
      if (nullptr != csr_port_heap) {
        DLOG(ERROR) << "Found multiple suitable CSR Port heaps";
        return nullptr;
      }
      csr_port_heap = handle;
    }
  }
  return csr_port_heap;
}

}  // namespace sandbox
