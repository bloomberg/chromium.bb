/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/wtf/typed_arrays/ArrayBufferContents.h"
#include "build/build_config.h"

#include <string.h>
#if defined(OS_LINUX)
#include "sandbox/linux/services/resource_limits.h"  // nogncheck
#endif
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/allocator/Partitions.h"

namespace WTF {

void ArrayBufferContents::DefaultAdjustAmountOfExternalAllocatedMemoryFunction(
    int64_t diff) {
  // Do nothing by default.
}

ArrayBufferContents::AdjustAmountOfExternalAllocatedMemoryFunction
    ArrayBufferContents::adjust_amount_of_external_allocated_memory_function_ =
        DefaultAdjustAmountOfExternalAllocatedMemoryFunction;

#if DCHECK_IS_ON()
ArrayBufferContents::AdjustAmountOfExternalAllocatedMemoryFunction
    ArrayBufferContents::
        last_used_adjust_amount_of_external_allocated_memory_function_;
#endif

ArrayBufferContents::ArrayBufferContents()
    : holder_(base::AdoptRef(new DataHolder())) {}

ArrayBufferContents::ArrayBufferContents(
    unsigned num_elements,
    unsigned element_byte_size,
    SharingType is_shared,
    ArrayBufferContents::InitializationPolicy policy)
    : holder_(base::AdoptRef(new DataHolder())) {
  // Do not allow 32-bit overflow of the total size.
  unsigned total_size = num_elements * element_byte_size;
  if (num_elements) {
    if (total_size / num_elements != element_byte_size) {
      return;
    }
  }

  holder_->AllocateNew(total_size, is_shared, policy);
}

ArrayBufferContents::ArrayBufferContents(DataHandle data,
                                         unsigned size_in_bytes,
                                         SharingType is_shared)
    : holder_(base::AdoptRef(new DataHolder())) {
  if (data) {
    holder_->Adopt(std::move(data), size_in_bytes, is_shared);
  } else {
    DCHECK_EQ(size_in_bytes, 0u);
    size_in_bytes = 0;
    // Allow null data if size is 0 bytes, make sure data is valid pointer.
    // (PartitionAlloc guarantees valid pointer for size 0)
    holder_->AllocateNew(size_in_bytes, is_shared, kZeroInitialize);
  }
}

ArrayBufferContents::~ArrayBufferContents() {}

void ArrayBufferContents::Neuter() {
  holder_ = nullptr;
}

void ArrayBufferContents::Transfer(ArrayBufferContents& other) {
  DCHECK(!IsShared());
  DCHECK(!other.holder_->Data());
  other.holder_ = holder_;
  Neuter();
}

void ArrayBufferContents::ShareWith(ArrayBufferContents& other) {
  DCHECK(IsShared());
  DCHECK(!other.holder_->Data());
  other.holder_ = holder_;
}

void ArrayBufferContents::CopyTo(ArrayBufferContents& other) {
  DCHECK(!holder_->IsShared() && !other.holder_->IsShared());
  other.holder_->CopyMemoryFrom(*holder_);
}

void* ArrayBufferContents::AllocateMemoryWithFlags(size_t size,
                                                   InitializationPolicy policy,
                                                   int flags) {
  void* data = PartitionAllocGenericFlags(
      Partitions::ArrayBufferPartition(), flags, size,
      WTF_HEAP_PROFILER_TYPE_NAME(ArrayBufferContents));
  if (policy == kZeroInitialize && data)
    memset(data, '\0', size);
  return data;
}

void* ArrayBufferContents::AllocateMemoryOrNull(size_t size,
                                                InitializationPolicy policy) {
  return AllocateMemoryWithFlags(size, policy, base::PartitionAllocReturnNull);
}

// This method is used by V8's WebAssembly implementation to reserve a large
// amount of inaccessible address space. This is used to enforce memory safety
// in Wasm programs.
void* ArrayBufferContents::ReserveMemory(size_t size) {
  void* const hint = nullptr;
  const size_t align = 64 << 10;  // Wasm page size

#if defined(OS_LINUX)
  // Linux by default has a small address space limit, which we chew up pretty
  // quickly with large memory reservations. To mitigate this, we bump up the
  // limit for array buffer reservations. See https://crbug.com/750378
  //
  // In general, returning nullptr is dangerous, as unsuspecting code may do an
  // offset-from-null and end up with an accessible but incorrect address.  This
  // function (ReserveMemory) is only used in contexts that expect allocation
  // may fail and explicitly handle the nullptr return case. This code is also
  // only used on 64-bit to create guard regions, which provides further
  // protection.
  if (!sandbox::ResourceLimits::AdjustCurrent(RLIMIT_AS, size)) {
    return nullptr;
  }
#endif

  constexpr bool commit = true;
  return base::AllocPages(hint, size, align, base::PageInaccessible, !commit);
}

void ArrayBufferContents::FreeMemory(void* data) {
  Partitions::ArrayBufferPartition()->Free(data);
}

void ArrayBufferContents::ReleaseReservedMemory(void* data, size_t size) {
#if defined(OS_LINUX)
  // Linux by default has a small address space limit, which we chew up pretty
  // quickly with large memory reservations. To mitigate this, we bump up the
  // limit for array buffer reservations. Here we need to lower it back down.
  // See https://crbug.com/750378
  CHECK(sandbox::ResourceLimits::AdjustCurrent(RLIMIT_AS, -size));
#endif
  base::FreePages(data, size);
}

ArrayBufferContents::DataHandle ArrayBufferContents::CreateDataHandle(
    size_t size,
    InitializationPolicy policy) {
  return DataHandle(ArrayBufferContents::AllocateMemoryOrNull(size, policy),
                    FreeMemory);
}

ArrayBufferContents::DataHolder::DataHolder()
    : data_(nullptr, FreeMemory),
      size_in_bytes_(0),
      is_shared_(kNotShared),
      has_registered_external_allocation_(false) {}

ArrayBufferContents::DataHolder::~DataHolder() {
  if (has_registered_external_allocation_)
    AdjustAmountOfExternalAllocatedMemory(
        -static_cast<int64_t>(size_in_bytes_));

  size_in_bytes_ = 0;
  is_shared_ = kNotShared;
}

void ArrayBufferContents::DataHolder::AllocateNew(unsigned size_in_bytes,
                                                  SharingType is_shared,
                                                  InitializationPolicy policy) {
  DCHECK(!data_);
  DCHECK_EQ(size_in_bytes_, 0u);
  DCHECK(!has_registered_external_allocation_);

  data_ = CreateDataHandle(size_in_bytes, policy);
  if (!data_)
    return;

  size_in_bytes_ = size_in_bytes;
  is_shared_ = is_shared;

  AdjustAmountOfExternalAllocatedMemory(size_in_bytes_);
}

void ArrayBufferContents::DataHolder::Adopt(DataHandle data,
                                            unsigned size_in_bytes,
                                            SharingType is_shared) {
  DCHECK(!data_);
  DCHECK_EQ(size_in_bytes_, 0u);
  DCHECK(!has_registered_external_allocation_);

  data_ = std::move(data);
  size_in_bytes_ = size_in_bytes;
  is_shared_ = is_shared;

  AdjustAmountOfExternalAllocatedMemory(size_in_bytes_);
}

void ArrayBufferContents::DataHolder::CopyMemoryFrom(const DataHolder& source) {
  DCHECK(!data_);
  DCHECK_EQ(size_in_bytes_, 0u);
  DCHECK(!has_registered_external_allocation_);

  data_ = CreateDataHandle(source.SizeInBytes(), kDontInitialize);
  if (!data_)
    return;

  size_in_bytes_ = source.SizeInBytes();
  memcpy(data_.Data(), source.Data(), source.SizeInBytes());

  AdjustAmountOfExternalAllocatedMemory(size_in_bytes_);
}

void ArrayBufferContents::DataHolder::
    RegisterExternalAllocationWithCurrentContext() {
  DCHECK(!has_registered_external_allocation_);
  AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(size_in_bytes_));
}

void ArrayBufferContents::DataHolder::
    UnregisterExternalAllocationWithCurrentContext() {
  if (!has_registered_external_allocation_)
    return;
  AdjustAmountOfExternalAllocatedMemory(-static_cast<int64_t>(size_in_bytes_));
}

}  // namespace WTF
