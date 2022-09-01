// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_region.h"

#include "base/bits.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/shared_memory_security_policy.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/checked_math.h"
#include "base/system/sys_info.h"

namespace base {
namespace subtle {

namespace {

void RecordMappingWasBlockedHistogram(bool blocked) {
  UmaHistogramBoolean("SharedMemory.MapBlockedForSecurity", blocked);
}

}  // namespace

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::CreateWritable(
    size_t size) {
  return Create(Mode::kWritable, size);
}

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::CreateUnsafe(
    size_t size) {
  return Create(Mode::kUnsafe, size);
}

PlatformSharedMemoryRegion::PlatformSharedMemoryRegion() = default;
PlatformSharedMemoryRegion::PlatformSharedMemoryRegion(
    PlatformSharedMemoryRegion&& other) = default;
PlatformSharedMemoryRegion& PlatformSharedMemoryRegion::operator=(
    PlatformSharedMemoryRegion&& other) = default;
PlatformSharedMemoryRegion::~PlatformSharedMemoryRegion() = default;

ScopedPlatformSharedMemoryHandle
PlatformSharedMemoryRegion::PassPlatformHandle() {
  return std::move(handle_);
}

absl::optional<span<uint8_t>> PlatformSharedMemoryRegion::MapAt(
    uint64_t offset,
    size_t size,
    SharedMemoryMapper* mapper) const {
  if (!IsValid())
    return absl::nullopt;

  if (size == 0)
    return absl::nullopt;

  size_t end_byte;
  if (!CheckAdd(offset, size).AssignIfValid(&end_byte) || end_byte > size_) {
    return absl::nullopt;
  }

  // TODO(dcheng): Presumably the actual size of the mapping is rounded to
  // `SysInfo::VMAllocationGranularity()`. Should this accounting be done with
  // that in mind?
  if (!SharedMemorySecurityPolicy::AcquireReservationForMapping(size)) {
    RecordMappingWasBlockedHistogram(/*blocked=*/true);
    return absl::nullopt;
  }

  RecordMappingWasBlockedHistogram(/*blocked=*/false);

  if (!mapper)
    mapper = SharedMemoryMapper::GetDefaultInstance();

  // The backing mapper expects offset to be aligned to
  // `SysInfo::VMAllocationGranularity()`.
  size_t aligned_offset =
      bits::AlignDown(offset, SysInfo::VMAllocationGranularity());
  size_t adjustment_for_alignment = offset - aligned_offset;

  bool write_allowed = mode_ != Mode::kReadOnly;
  auto result = mapper->Map(GetPlatformHandle(), write_allowed, aligned_offset,
                            size + adjustment_for_alignment);

  if (result.has_value()) {
    DCHECK(IsAligned(result.value().data(), kMapMinimumAlignment));
    if (offset != 0) {
      // Undo the previous adjustment so the returned mapping respects the exact
      // requested `offset` and `size`.
      result = result->subspan(adjustment_for_alignment);
    }
  } else {
    SharedMemorySecurityPolicy::ReleaseReservationForMapping(size);
  }

  return result;
}

}  // namespace subtle
}  // namespace base
