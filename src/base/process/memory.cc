// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/memory.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#else
#include <unistd.h>
#endif  // BUILDFLAG(IS_WIN)

#include <string.h>

#include "base/allocator/buildflags.h"
#include "base/cxx17_backports.h"
#include "base/debug/alias.h"
#include "base/immediate_crash.h"
#include "base/logging.h"
#if BUILDFLAG(USE_PARTITION_ALLOC)
#include "base/allocator/partition_allocator/page_allocator.h"
#endif
#include "build/build_config.h"

namespace base {

// Defined in memory_mac.mm for macOS + use_allocator="none".  In case of
// USE_PARTITION_ALLOC_AS_MALLOC, no need to route the call to the system
// default calloc of macOS.
#if !BUILDFLAG(IS_APPLE) || BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

bool UncheckedCalloc(size_t num_items, size_t size, void** result) {
  const size_t alloc_size = num_items * size;

  // Overflow check
  if (size && ((alloc_size / size) != num_items)) {
    *result = nullptr;
    return false;
  }

  if (!UncheckedMalloc(alloc_size, result))
    return false;

  memset(*result, 0, alloc_size);
  return true;
}

#endif  // !BUILDFLAG(IS_APPLE) || BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace internal {
bool ReleaseAddressSpaceReservation() {
#if BUILDFLAG(USE_PARTITION_ALLOC)
  return ReleaseReservation();
#else
  return false;
#endif
}
}  // namespace internal

}  // namespace base
