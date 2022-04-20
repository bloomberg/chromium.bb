// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_BITMAP_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_BITMAP_H_

#include <array>
#include <atomic>
#include <bitset>
#include <limits>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/base_export.h"
#include "build/build_config.h"

#if !defined(PA_HAS_64_BITS_POINTERS)

namespace partition_alloc {

namespace internal {

// AddressPoolManagerBitmap is a set of bitmaps that track whether a given
// address is in a pool that supports BackupRefPtr, or in a pool that doesn't
// support it. All PartitionAlloc allocations must be in either of the pools.
//
// This code is specific to 32-bit systems.
class BASE_EXPORT AddressPoolManagerBitmap {
 public:
  static constexpr uint64_t kGiB = 1024 * 1024 * 1024ull;
  static constexpr uint64_t kAddressSpaceSize = 4ull * kGiB;

  // For BRP pool, we use partition page granularity to eliminate the guard
  // pages from the bitmap at the ends:
  // - Eliminating the guard page at the beginning is needed so that pointers
  //   to the end of an allocation that immediately precede a super page in BRP
  //   pool don't accidentally fall into that pool.
  // - Eliminating the guard page at the end is to ensure that the last page
  //   of the address space isn't in the BRP pool. This allows using sentinels
  //   like reinterpret_cast<void*>(-1) without a risk of triggering BRP logic
  //   on an invalid address. (Note, 64-bit systems don't have this problem as
  //   the upper half of the address space always belongs to the OS.)
  //
  // Note, direct map allocations also belong to this pool. The same logic as
  // above applies. It is important to note, however, that the granularity used
  // here has to be a minimum of partition page size and direct map allocation
  // granularity. Since DirectMapAllocationGranularity() is no smaller than
  // PageAllocationGranularity(), we don't need to decrease the bitmap
  // granularity any further.
  static constexpr size_t kBitShiftOfBRPPoolBitmap = PartitionPageShift();
  static constexpr size_t kBytesPer1BitOfBRPPoolBitmap = PartitionPageSize();
  static_assert(kBytesPer1BitOfBRPPoolBitmap == 1 << kBitShiftOfBRPPoolBitmap,
                "");
  static constexpr size_t kGuardOffsetOfBRPPoolBitmap = 1;
  static constexpr size_t kGuardBitsOfBRPPoolBitmap = 2;
  static constexpr size_t kBRPPoolBits =
      kAddressSpaceSize / kBytesPer1BitOfBRPPoolBitmap;

  // Regular pool may include both normal bucket and direct map allocations, so
  // the bitmap granularity has to be at least as small as
  // DirectMapAllocationGranularity(). No need to eliminate guard pages at the
  // ends, as this is a BackupRefPtr-specific concern, hence no need to lower
  // the granularity to partition page size.
  static constexpr size_t kBitShiftOfRegularPoolBitmap =
      DirectMapAllocationGranularityShift();
  static constexpr size_t kBytesPer1BitOfRegularPoolBitmap =
      DirectMapAllocationGranularity();
  static_assert(kBytesPer1BitOfRegularPoolBitmap ==
                    1 << kBitShiftOfRegularPoolBitmap,
                "");
  static constexpr size_t kRegularPoolBits =
      kAddressSpaceSize / kBytesPer1BitOfRegularPoolBitmap;

  // Returns false for nullptr.
  static bool IsManagedByRegularPool(uintptr_t address) {
    static_assert(
        std::numeric_limits<uintptr_t>::max() >> kBitShiftOfRegularPoolBitmap <
            regular_pool_bits_.size(),
        "The bitmap is too small, will result in unchecked out of bounds "
        "accesses.");
    // It is safe to read |regular_pool_bits_| without a lock since the caller
    // is responsible for guaranteeing that the address is inside a valid
    // allocation and the deallocation call won't race with this call.
    return TS_UNCHECKED_READ(
        regular_pool_bits_)[address >> kBitShiftOfRegularPoolBitmap];
  }

  // Returns false for nullptr.
  static bool IsManagedByBRPPool(uintptr_t address) {
    static_assert(std::numeric_limits<uintptr_t>::max() >>
                      kBitShiftOfBRPPoolBitmap < brp_pool_bits_.size(),
                  "The bitmap is too small, will result in unchecked out of "
                  "bounds accesses.");
    // It is safe to read |brp_pool_bits_| without a lock since the caller
    // is responsible for guaranteeing that the address is inside a valid
    // allocation and the deallocation call won't race with this call.
    return TS_UNCHECKED_READ(
        brp_pool_bits_)[address >> kBitShiftOfBRPPoolBitmap];
  }

#if BUILDFLAG(USE_BACKUP_REF_PTR)
  static void BanSuperPageFromBRPPool(uintptr_t address) {
    brp_forbidden_super_page_map_[address >> kSuperPageShift].store(
        true, std::memory_order_relaxed);
  }

  static bool IsAllowedSuperPageForBRPPool(uintptr_t address) {
    // The only potentially dangerous scenario, in which this check is used, is
    // when the assignment of the first raw_ptr<T> object for a non-GigaCage
    // address is racing with the allocation of a new GigCage super-page at the
    // same address. We assume that if raw_ptr<T> is being initialized with a
    // raw pointer, the associated allocation is "alive"; otherwise, the issue
    // should be fixed by rewriting the raw pointer variable as raw_ptr<T>.
    // In the worst case, when such a fix is impossible, we should just undo the
    // raw pointer -> raw_ptr<T> rewrite of the problematic field. If the
    // above assumption holds, the existing allocation will prevent us from
    // reserving the super-page region and, thus, having the race condition.
    // Since we rely on that external synchronization, the relaxed memory
    // ordering should be sufficient.
    return !brp_forbidden_super_page_map_[address >> kSuperPageShift].load(
        std::memory_order_relaxed);
  }

  static void IncrementBlocklistHitCount() { ++blocklist_hit_count_; }
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

 private:
  friend class AddressPoolManager;

  static Lock& GetLock();

  static std::bitset<kRegularPoolBits> regular_pool_bits_ GUARDED_BY(GetLock());
  static std::bitset<kBRPPoolBits> brp_pool_bits_ GUARDED_BY(GetLock());
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  static std::array<std::atomic_bool, kAddressSpaceSize / kSuperPageSize>
      brp_forbidden_super_page_map_;
  static std::atomic_size_t blocklist_hit_count_;
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
};

}  // namespace internal

// Returns false for nullptr.
ALWAYS_INLINE bool IsManagedByPartitionAlloc(uintptr_t address) {
  // When USE_BACKUP_REF_PTR is off, BRP pool isn't used.
  // No need to add IsManagedByConfigurablePool, because Configurable Pool
  // doesn't exist on 32-bit.
#if !BUILDFLAG(USE_BACKUP_REF_PTR)
  PA_DCHECK(!internal::AddressPoolManagerBitmap::IsManagedByBRPPool(address));
#endif
  return internal::AddressPoolManagerBitmap::IsManagedByRegularPool(address)
#if BUILDFLAG(USE_BACKUP_REF_PTR)
         || internal::AddressPoolManagerBitmap::IsManagedByBRPPool(address)
#endif
      ;
}

// Returns false for nullptr.
ALWAYS_INLINE bool IsManagedByPartitionAllocRegularPool(uintptr_t address) {
  return internal::AddressPoolManagerBitmap::IsManagedByRegularPool(address);
}

// Returns false for nullptr.
ALWAYS_INLINE bool IsManagedByPartitionAllocBRPPool(uintptr_t address) {
  return internal::AddressPoolManagerBitmap::IsManagedByBRPPool(address);
}

// Returns false for nullptr.
ALWAYS_INLINE bool IsManagedByPartitionAllocConfigurablePool(
    uintptr_t address) {
  // The Configurable Pool is only available on 64-bit builds.
  return false;
}

ALWAYS_INLINE bool IsConfigurablePoolAvailable() {
  // The Configurable Pool is only available on 64-bit builds.
  return false;
}

}  // namespace partition_alloc

namespace base {

// TODO(https://crbug.com/1288247): Remove these 'using' declarations once
// the migration to the new namespaces gets done.
using ::partition_alloc::IsConfigurablePoolAvailable;
using ::partition_alloc::IsManagedByPartitionAlloc;
using ::partition_alloc::IsManagedByPartitionAllocBRPPool;
using ::partition_alloc::IsManagedByPartitionAllocConfigurablePool;
using ::partition_alloc::IsManagedByPartitionAllocRegularPool;

namespace internal {

using ::partition_alloc::internal::AddressPoolManagerBitmap;

}  // namespace internal

}  // namespace base

#endif  // !defined(PA_HAS_64_BITS_POINTERS)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_POOL_MANAGER_BITMAP_H_
