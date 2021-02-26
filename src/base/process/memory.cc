// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/memory.h"

#if defined(OS_WIN)
#include <windows.h>
#else
#include <unistd.h>
#endif  // defined(OS_WIN)

#include <string.h>

#include "base/allocator/buildflags.h"
#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/partition_alloc_buildflags.h"
#if BUILDFLAG(USE_PARTITION_ALLOC)
#include "base/allocator/partition_allocator/page_allocator.h"
#endif
#include "build/build_config.h"

namespace base {

namespace internal {

NOINLINE void OnNoMemoryInternal(size_t size) {
#if defined(OS_WIN)
  // Kill the process. This is important for security since most of code
  // does not check the result of memory allocation.
  // https://msdn.microsoft.com/en-us/library/het71c37.aspx
  // Pass the size of the failed request in an exception argument.
  ULONG_PTR exception_args[] = {size};
  ::RaiseException(base::win::kOomExceptionCode, EXCEPTION_NONCONTINUABLE,
                   base::size(exception_args), exception_args);

  // Safety check, make sure process exits here.
  _exit(win::kOomExceptionCode);
#else
  size_t tmp_size = size;
  base::debug::Alias(&tmp_size);

  // Note: Don't add anything that may allocate here. Depending on the
  // allocator, this may be called from within the allocator (e.g. with
  // PartitionAlloc), and would deadlock as our locks are not recursive.
  //
  // Additionally, this is unlikely to work, since allocating from an OOM
  // handler is likely to fail.
  abort();  // SIGABRT cannot really be caught, this will always _exit().

#endif  // defined(OS_WIN)
}

}  // namespace internal

// Defined in memory_win.cc for Windows.
#if !defined(OS_WIN)

namespace {

// Breakpad server classifies base::`anonymous namespace'::OnNoMemory as
// out-of-memory crash.
NOINLINE void OnNoMemory(size_t size) {
  internal::OnNoMemoryInternal(size);
}

}  // namespace

void TerminateBecauseOutOfMemory(size_t size) {
  OnNoMemory(size);
}

#endif  // !defined(OS_WIN)

// Defined in memory_mac.mm for Mac.
#if !defined(OS_APPLE)

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

#endif  // defined(OS_APPLE)

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
