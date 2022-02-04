// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v8_platform_page_allocator.h"

#include "base/allocator/partition_allocator/address_space_randomization.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/random.h"
#include "base/check_op.h"
#include "base/cpu.h"
#include "build/build_config.h"

namespace {

// Maps the v8 page permissions into a page configuration from base.
base::PageAccessibilityConfiguration GetPageConfig(
    v8::PageAllocator::Permission permission) {
  switch (permission) {
    case v8::PageAllocator::Permission::kRead:
      return base::PageRead;
    case v8::PageAllocator::Permission::kReadWrite:
      return base::PageReadWrite;
    case v8::PageAllocator::Permission::kReadWriteExecute:
      // at the moment bti-protection is not enabled for this path since some
      // projects may still be using non-bti compliant code.
      return base::PageReadWriteExecute;
    case v8::PageAllocator::Permission::kReadExecute:
#if defined(__ARM_FEATURE_BTI_DEFAULT)
      return base::CPU::GetInstanceNoAllocation().has_bti()
                 ? base::PageReadExecuteProtected
                 : base::PageReadExecute;
#else
      return base::PageReadExecute;
#endif
    case v8::PageAllocator::Permission::kNoAccessWillJitLater:
      // We could use this information to conditionally set the MAP_JIT flag
      // on Mac-arm64; however this permissions value is intended to be a
      // short-term solution, so we continue to set MAP_JIT for all V8 pages
      // for now.
      return base::PageInaccessible;
    default:
      DCHECK_EQ(v8::PageAllocator::Permission::kNoAccess, permission);
      return base::PageInaccessible;
  }
}

}  // namespace

namespace gin {
PageAllocator::~PageAllocator() = default;

size_t PageAllocator::AllocatePageSize() {
  return base::PageAllocationGranularity();
}

size_t PageAllocator::CommitPageSize() {
  return base::SystemPageSize();
}

void PageAllocator::SetRandomMmapSeed(int64_t seed) {
  base::SetMmapSeedForTesting(seed);
}

void* PageAllocator::GetRandomMmapAddr() {
  return reinterpret_cast<void*>(base::GetRandomPageBase());
}

void* PageAllocator::AllocatePages(void* address,
                                   size_t length,
                                   size_t alignment,
                                   v8::PageAllocator::Permission permissions) {
  base::PageAccessibilityConfiguration config = GetPageConfig(permissions);
  return base::AllocPages(address, length, alignment, config,
                          base::PageTag::kV8);
}

bool PageAllocator::FreePages(void* address, size_t length) {
  base::FreePages(address, length);
  return true;
}

bool PageAllocator::ReleasePages(void* address,
                                 size_t length,
                                 size_t new_length) {
  DCHECK_LT(new_length, length);
  uint8_t* release_base = reinterpret_cast<uint8_t*>(address) + new_length;
  size_t release_size = length - new_length;
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  // On POSIX, we can unmap the trailing pages.
  base::FreePages(release_base, release_size);
#elif BUILDFLAG(IS_WIN)
  // On Windows, we can only de-commit the trailing pages. FreePages() will
  // still free all pages in the region including the released tail, so it's
  // safe to just decommit the tail.
  base::DecommitSystemPages(release_base, release_size,
                            base::PageUpdatePermissions);
#else
#error Unsupported platform
#endif
  return true;
}

bool PageAllocator::SetPermissions(void* address,
                                   size_t length,
                                   Permission permissions) {
  // If V8 sets permissions to none, we can discard the memory.
  if (permissions == v8::PageAllocator::Permission::kNoAccess) {
    // Use PageKeepPermissionsIfPossible as an optimization, to avoid perf
    // regression (see crrev.com/c/2563038 for details). This may cause the
    // memory region to still be accessible on certain platforms, but at least
    // the physical pages will be discarded.
    base::DecommitSystemPages(address, length,
                              base::PageKeepPermissionsIfPossible);
    return true;
  } else {
    return base::TrySetSystemPagesAccess(address, length,
                                         GetPageConfig(permissions));
  }
}

bool PageAllocator::DiscardSystemPages(void* address, size_t size) {
  base::DiscardSystemPages(address, size);
  return true;
}

bool PageAllocator::DecommitPages(void* address, size_t size) {
  // V8 expects the pages to be inaccessible and zero-initialized upon next
  // access.
  base::DecommitAndZeroSystemPages(address, size);
  return true;
}

base::PageAccessibilityConfiguration PageAllocator::GetPageConfigForTesting(
    v8::PageAllocator::Permission permission) {
  return GetPageConfig(permission);
}

}  // namespace gin
