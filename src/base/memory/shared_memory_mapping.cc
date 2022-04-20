// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_mapping.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/platform_shared_memory_mapper.h"
#include "base/memory/shared_memory_security_policy.h"
#include "base/memory/shared_memory_tracker.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"

namespace base {

SharedMemoryMapping::SharedMemoryMapping() = default;

SharedMemoryMapping::SharedMemoryMapping(SharedMemoryMapping&& mapping) noexcept
    : memory_(std::exchange(mapping.memory_, nullptr)),
      size_(mapping.size_),
      mapped_size_(mapping.mapped_size_),
      guid_(mapping.guid_) {}

SharedMemoryMapping& SharedMemoryMapping::operator=(
    SharedMemoryMapping&& mapping) noexcept {
  Unmap();
  memory_ = std::exchange(mapping.memory_, nullptr);
  size_ = mapping.size_;
  mapped_size_ = mapping.mapped_size_;
  guid_ = mapping.guid_;
  return *this;
}

SharedMemoryMapping::~SharedMemoryMapping() {
  Unmap();
}

SharedMemoryMapping::SharedMemoryMapping(void* memory,
                                         size_t size,
                                         size_t mapped_size,
                                         const UnguessableToken& guid)
    : memory_(memory), size_(size), mapped_size_(mapped_size), guid_(guid) {
  SharedMemoryTracker::GetInstance()->IncrementMemoryUsage(*this);
}

void SharedMemoryMapping::Unmap() {
  if (!IsValid())
    return;

  SharedMemorySecurityPolicy::ReleaseReservationForMapping(size_);
  SharedMemoryTracker::GetInstance()->DecrementMemoryUsage(*this);

  PlatformSharedMemoryMapper::Unmap(memory_, mapped_size_);
}

ReadOnlySharedMemoryMapping::ReadOnlySharedMemoryMapping() = default;
ReadOnlySharedMemoryMapping::ReadOnlySharedMemoryMapping(
    ReadOnlySharedMemoryMapping&&) noexcept = default;
ReadOnlySharedMemoryMapping& ReadOnlySharedMemoryMapping::operator=(
    ReadOnlySharedMemoryMapping&&) noexcept = default;
ReadOnlySharedMemoryMapping::ReadOnlySharedMemoryMapping(
    void* address,
    size_t size,
    size_t mapped_size,
    const UnguessableToken& guid)
    : SharedMemoryMapping(address, size, mapped_size, guid) {}

WritableSharedMemoryMapping::WritableSharedMemoryMapping() = default;
WritableSharedMemoryMapping::WritableSharedMemoryMapping(
    WritableSharedMemoryMapping&&) noexcept = default;
WritableSharedMemoryMapping& WritableSharedMemoryMapping::operator=(
    WritableSharedMemoryMapping&&) noexcept = default;
WritableSharedMemoryMapping::WritableSharedMemoryMapping(
    void* address,
    size_t size,
    size_t mapped_size,
    const UnguessableToken& guid)
    : SharedMemoryMapping(address, size, mapped_size, guid) {}

}  // namespace base
