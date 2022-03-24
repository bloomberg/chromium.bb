// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_RESERVATION_OFFSET_TABLE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_RESERVATION_OFFSET_TABLE_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <tuple>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

namespace base {
namespace internal {

static constexpr uint16_t kOffsetTagNotAllocated =
    std::numeric_limits<uint16_t>::max();
static constexpr uint16_t kOffsetTagNormalBuckets =
    std::numeric_limits<uint16_t>::max() - 1;

// The main purpose of the reservation offset table is to easily locate the
// direct map reservation start address for any given address. There is one
// entry in the table for each super page.
//
// When PartitionAlloc reserves an address region it is always aligned to
// super page boundary. However, in 32-bit mode, the size may not be aligned
// super-page-aligned, so it may look like this:
//   |<--------- actual reservation size --------->|
//   +----------+----------+-----+-----------+-----+ - - - +
//   |SuperPage0|SuperPage1| ... |SuperPage K|SuperPage K+1|
//   +----------+----------+-----+-----------+-----+ - - -.+
//                                           |<-X->|<-Y*)->|
//
// The table entries for reserved super pages say how many pages away from the
// reservation the super page is:
//   +----------+----------+-----+-----------+-------------+
//   |Entry for |Entry for | ... |Entry for  |Entry for    |
//   |SuperPage0|SuperPage1|     |SuperPage K|SuperPage K+1|
//   +----------+----------+-----+-----------+-------------+
//   |     0    |    1     | ... |     K     |   K + 1     |
//   +----------+----------+-----+-----------+-------------+
//
// For an address Z, the reservation start can be found using this formula:
//   ((Z >> kSuperPageShift) - (the entry for Z)) << kSuperPageShift
//
// kOffsetTagNotAllocated is a special tag denoting that the super page isn't
// allocated by PartitionAlloc and kOffsetTagNormalBuckets denotes that it is
// used for a normal-bucket allocation, not for a direct-map allocation.
//
// *) In 32-bit mode, Y is not used by PartitionAlloc, and cannot be used
//    until X is unreserved, because PartitionAlloc always uses kSuperPageSize
//    alignment when reserving address spaces. One can use "GigaCage" to
//    further determine which part of the supe page is used by PartitionAlloc.
//    This isn't a problem in 64-bit mode, where allocation granularity is
//    kSuperPageSize.
class BASE_EXPORT ReservationOffsetTable {
 public:
#if defined(PA_HAS_64_BITS_POINTERS)
  // There is one reservation offset table per Pool in 64-bit mode.
  static constexpr size_t kReservationOffsetTableCoverage = kPoolMaxSize;
  static constexpr size_t kReservationOffsetTableLength =
      kReservationOffsetTableCoverage >> kSuperPageShift;
#else
  // The size of the reservation offset table should cover the entire 32-bit
  // address space, one element per super page.
  static constexpr uint64_t kGiB = 1024 * 1024 * 1024ull;
  static constexpr size_t kReservationOffsetTableLength =
      4 * kGiB / kSuperPageSize;
#endif
  static_assert(kReservationOffsetTableLength < kOffsetTagNormalBuckets,
                "Offsets should be smaller than kOffsetTagNormalBuckets.");

  static struct _ReservationOffsetTable {
    // The number of table elements is less than MAX_UINT16, so the element type
    // can be uint16_t.
    static_assert(
        kReservationOffsetTableLength <= std::numeric_limits<uint16_t>::max(),
        "Length of the reservation offset table must be less than MAX_UINT16");
    uint16_t offsets[kReservationOffsetTableLength] = {};

    constexpr _ReservationOffsetTable() {
      for (uint16_t& offset : offsets)
        offset = kOffsetTagNotAllocated;
    }
#if defined(PA_HAS_64_BITS_POINTERS)
    // One table per Pool.
  } reservation_offset_tables_[kNumPools];
#else
    // A single table for the entire 32-bit address space.
  } reservation_offset_table_;
#endif
};

#if defined(PA_HAS_64_BITS_POINTERS)
ALWAYS_INLINE uint16_t* GetReservationOffsetTable(pool_handle handle) {
  PA_DCHECK(0 < handle && handle <= kNumPools);
  return ReservationOffsetTable::reservation_offset_tables_[handle - 1].offsets;
}

ALWAYS_INLINE const uint16_t* GetReservationOffsetTableEnd(pool_handle handle) {
  return GetReservationOffsetTable(handle) +
         ReservationOffsetTable::kReservationOffsetTableLength;
}

ALWAYS_INLINE uint16_t* GetReservationOffsetTable(uintptr_t address) {
  pool_handle handle = GetPool(address);
  return GetReservationOffsetTable(handle);
}

ALWAYS_INLINE const uint16_t* GetReservationOffsetTableEnd(uintptr_t address) {
  pool_handle handle = GetPool(address);
  return GetReservationOffsetTableEnd(handle);
}

ALWAYS_INLINE uint16_t* ReservationOffsetPointer(pool_handle pool,
                                                 uintptr_t offset_in_pool) {
  size_t table_index = offset_in_pool >> kSuperPageShift;
  PA_DCHECK(table_index <
            ReservationOffsetTable::kReservationOffsetTableLength);
  return GetReservationOffsetTable(pool) + table_index;
}
#else
ALWAYS_INLINE uint16_t* GetReservationOffsetTable(uintptr_t address) {
  return ReservationOffsetTable::reservation_offset_table_.offsets;
}

ALWAYS_INLINE const uint16_t* GetReservationOffsetTableEnd(uintptr_t address) {
  return ReservationOffsetTable::reservation_offset_table_.offsets +
         ReservationOffsetTable::kReservationOffsetTableLength;
}
#endif

ALWAYS_INLINE uint16_t* ReservationOffsetPointer(uintptr_t address) {
#if defined(PA_HAS_64_BITS_POINTERS)
  // In 64-bit mode, find the owning Pool and compute the offset from its base.
  pool_handle pool;
  address = memory::UnmaskPtr(address);
  uintptr_t offset;
  std::tie(pool, offset) = GetPoolAndOffset(address);
  return ReservationOffsetPointer(pool, offset);
#else
  size_t table_index = address >> kSuperPageShift;
  PA_DCHECK(table_index <
            ReservationOffsetTable::kReservationOffsetTableLength);
  return GetReservationOffsetTable(address) + table_index;
#endif
}

ALWAYS_INLINE uintptr_t ComputeReservationStart(uintptr_t address,
                                                uint16_t* offset_ptr) {
  return (address & kSuperPageBaseMask) -
         (static_cast<size_t>(*offset_ptr) << kSuperPageShift);
}

// If the given address doesn't point to direct-map allocated memory,
// returns 0.
ALWAYS_INLINE uintptr_t GetDirectMapReservationStart(uintptr_t address) {
#if DCHECK_IS_ON()
  bool is_in_brp_pool = IsManagedByPartitionAllocBRPPool(address);
  bool is_in_regular_pool = IsManagedByPartitionAllocRegularPool(address);
  // When USE_BACKUP_REF_PTR is off, BRP pool isn't used.
#if !BUILDFLAG(USE_BACKUP_REF_PTR)
  PA_DCHECK(!is_in_brp_pool);
#endif
#endif  // DCHECK_IS_ON()
  uint16_t* offset_ptr = ReservationOffsetPointer(address);
  PA_DCHECK(*offset_ptr != kOffsetTagNotAllocated);
  if (*offset_ptr == kOffsetTagNormalBuckets)
    return 0;
  uintptr_t reservation_start = ComputeReservationStart(address, offset_ptr);
#if DCHECK_IS_ON()
  // Make sure the reservation start is in the same pool as |address|.
  // In the 32-bit mode, the beginning of a reservation may be excluded from the
  // BRP pool, so shift the pointer. The other pools don't have this logic.
  PA_DCHECK(is_in_brp_pool ==
            IsManagedByPartitionAllocBRPPool(
                reservation_start
#if !defined(PA_HAS_64_BITS_POINTERS)
                + AddressPoolManagerBitmap::kBytesPer1BitOfBRPPoolBitmap *
                      AddressPoolManagerBitmap::kGuardOffsetOfBRPPoolBitmap
#endif  // !defined(PA_HAS_64_BITS_POINTERS)
                ));
  PA_DCHECK(is_in_regular_pool ==
            IsManagedByPartitionAllocRegularPool(reservation_start));
  PA_DCHECK(*ReservationOffsetPointer(reservation_start) == 0);
#endif  // DCHECK_IS_ON()

  return reservation_start;
}

#if defined(PA_HAS_64_BITS_POINTERS)
// If the given address doesn't point to direct-map allocated memory,
// returns 0.
// This variant has better performance than the regular one on 64-bit builds if
// the Pool that an allocation belongs to is known.
ALWAYS_INLINE uintptr_t GetDirectMapReservationStart(uintptr_t address,
                                                     pool_handle pool,
                                                     uintptr_t offset_in_pool) {
  PA_DCHECK(AddressPoolManager::GetInstance()->GetPoolBaseAddress(pool) +
                offset_in_pool ==
            address);
  uint16_t* offset_ptr = ReservationOffsetPointer(pool, offset_in_pool);
  PA_DCHECK(*offset_ptr != kOffsetTagNotAllocated);
  if (*offset_ptr == kOffsetTagNormalBuckets)
    return 0;
  uintptr_t reservation_start = ComputeReservationStart(address, offset_ptr);
  PA_DCHECK(*ReservationOffsetPointer(reservation_start) == 0);
  return reservation_start;
}
#endif  // defined(PA_HAS_64_BITS_POINTERS)

// Returns true if |address| is the beginning of the first super page of a
// reservation, i.e. either a normal bucket super page, or the first super page
// of direct map.
// |address| must belong to an allocated super page.
ALWAYS_INLINE bool IsReservationStart(uintptr_t address) {
  uint16_t* offset_ptr = ReservationOffsetPointer(address);
  PA_DCHECK(*offset_ptr != kOffsetTagNotAllocated);
  return ((*offset_ptr == kOffsetTagNormalBuckets) || (*offset_ptr == 0)) &&
         (address % kSuperPageSize == 0);
}

// Returns true if |address| belongs to a normal bucket super page.
ALWAYS_INLINE bool IsManagedByNormalBuckets(uintptr_t address) {
  uint16_t* offset_ptr = ReservationOffsetPointer(address);
  return *offset_ptr == kOffsetTagNormalBuckets;
}

// Returns true if |address| belongs to a direct map region.
ALWAYS_INLINE bool IsManagedByDirectMap(uintptr_t address) {
  uint16_t* offset_ptr = ReservationOffsetPointer(address);
  return *offset_ptr != kOffsetTagNormalBuckets &&
         *offset_ptr != kOffsetTagNotAllocated;
}

// Returns true if |address| belongs to a normal bucket super page or a direct
// map region, i.e. belongs to an allocated super page.
ALWAYS_INLINE bool IsManagedByNormalBucketsOrDirectMap(uintptr_t address) {
  uint16_t* offset_ptr = ReservationOffsetPointer(address);
  return *offset_ptr != kOffsetTagNotAllocated;
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_RESERVATION_OFFSET_TABLE_H_
