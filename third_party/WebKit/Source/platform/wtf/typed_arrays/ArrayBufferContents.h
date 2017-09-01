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

#ifndef ArrayBufferContents_h
#define ArrayBufferContents_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/RefPtr.h"
#include "platform/wtf/ThreadSafeRefCounted.h"
#include "platform/wtf/WTF.h"
#include "platform/wtf/WTFExport.h"

namespace WTF {

class WTF_EXPORT ArrayBufferContents {
  WTF_MAKE_NONCOPYABLE(ArrayBufferContents);
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  using AdjustAmountOfExternalAllocatedMemoryFunction = void (*)(int64_t diff);
  // Types that need to be used when injecting external memory.
  // DataHandle allows specifying a deleter which will be invoked when
  // DataHandle instance goes out of scope. If the data memory is allocated
  // using ArrayBufferContents::allocateMemoryOrNull, it is necessary to
  // specify ArrayBufferContents::freeMemory as the DataDeleter.
  // Most clients would want to use ArrayBufferContents::createData, which
  // allocates memory and specifies the correct deleter.
  using DataDeleter = void (*)(void* data);

  enum class AllocationKind { kNormal, kReservation };

  class DataHandle {
    WTF_MAKE_NONCOPYABLE(DataHandle);

   public:
    DataHandle(void* data, DataDeleter deleter)
        : allocation_base_(data),
          allocation_length_(0),
          data_(data),
          data_length_(0),
          kind_(AllocationKind::kNormal),
          deleter_(deleter) {}
    DataHandle(void* allocation_base,
               size_t allocation_length,
               void* data,
               size_t data_length,
               AllocationKind kind,
               DataDeleter deleter)
        : allocation_base_(allocation_base),
          allocation_length_(allocation_length),
          data_(data),
          data_length_(data_length),
          kind_(kind),
          deleter_(deleter) {
      DCHECK(reinterpret_cast<uintptr_t>(allocation_base_) <=
             reinterpret_cast<uintptr_t>(data_));
      DCHECK(reinterpret_cast<uintptr_t>(data_) + data_length_ <=
             reinterpret_cast<uintptr_t>(allocation_base_) +
                 allocation_length_);
    }
    // Move constructor
    DataHandle(DataHandle&& other) { *this = std::move(other); }
    ~DataHandle() {
      if (!allocation_base_)
        return;
      DCHECK(reinterpret_cast<uintptr_t>(allocation_base_) <=
             reinterpret_cast<uintptr_t>(data_));
      DCHECK(reinterpret_cast<uintptr_t>(data_) + data_length_ <=
             reinterpret_cast<uintptr_t>(allocation_base_) +
                 allocation_length_);
      switch (kind_) {
        case AllocationKind::kNormal:
          DCHECK(deleter_);
          deleter_(data_);
          return;
        case AllocationKind::kReservation:
          ReleaseReservedMemory(allocation_base_, allocation_length_);
          return;
      }
    }

    // Move operator
    DataHandle& operator=(DataHandle&& other) {
      allocation_base_ = other.allocation_base_;
      allocation_length_ = other.allocation_length_;
      data_ = other.data_;
      data_length_ = other.data_length_;
      kind_ = other.kind_;
      deleter_ = other.deleter_;
      other.allocation_base_ = nullptr;
      return *this;
    }

    void* AllocationBase() const { return allocation_base_; }
    size_t AllocationLength() const { return allocation_length_; }

    void* Data() const { return data_; }
    size_t DataLength() const { return data_length_; }

    ArrayBufferContents::AllocationKind GetAllocationKind() const {
      return kind_;
    }

    operator bool() const { return allocation_base_; }

   private:
    void* allocation_base_;
    size_t allocation_length_;

    void* data_;
    size_t data_length_;

    ArrayBufferContents::AllocationKind kind_;
    DataDeleter deleter_;
  };

  enum InitializationPolicy { kZeroInitialize, kDontInitialize };

  enum SharingType {
    kNotShared,
    kShared,
  };

  ArrayBufferContents();
  ArrayBufferContents(unsigned num_elements,
                      unsigned element_byte_size,
                      SharingType is_shared,
                      InitializationPolicy);
  ArrayBufferContents(DataHandle,
                      unsigned size_in_bytes,
                      SharingType is_shared);
  ArrayBufferContents(ArrayBufferContents&&) = default;

  ~ArrayBufferContents();

  ArrayBufferContents& operator=(ArrayBufferContents&&) = default;

  void Neuter();

  void* Data() const {
    DCHECK(!IsShared());
    return DataMaybeShared();
  }
  void* DataShared() const {
    DCHECK(IsShared());
    return DataMaybeShared();
  }
  void* DataMaybeShared() const { return holder_ ? holder_->Data() : nullptr; }
  unsigned SizeInBytes() const { return holder_ ? holder_->SizeInBytes() : 0; }
  bool IsShared() const { return holder_ ? holder_->IsShared() : false; }

  void Transfer(ArrayBufferContents& other);
  void ShareWith(ArrayBufferContents& other);
  void CopyTo(ArrayBufferContents& other);

  static void* AllocateMemoryOrNull(size_t, InitializationPolicy);
  static void* ReserveMemory(size_t);
  static void FreeMemory(void*);
  static void ReleaseReservedMemory(void*, size_t);
  static DataHandle CreateDataHandle(size_t, InitializationPolicy);
  static void Initialize(
      AdjustAmountOfExternalAllocatedMemoryFunction function) {
    DCHECK(IsMainThread());
    DCHECK_EQ(adjust_amount_of_external_allocated_memory_function_,
              DefaultAdjustAmountOfExternalAllocatedMemoryFunction);
    adjust_amount_of_external_allocated_memory_function_ = function;
  }

  void RegisterExternalAllocationWithCurrentContext() {
    if (holder_)
      holder_->RegisterExternalAllocationWithCurrentContext();
  }

  void UnregisterExternalAllocationWithCurrentContext() {
    if (holder_)
      holder_->UnregisterExternalAllocationWithCurrentContext();
  }

 private:
  static void* AllocateMemoryWithFlags(size_t, InitializationPolicy, int);

  static void DefaultAdjustAmountOfExternalAllocatedMemoryFunction(
      int64_t diff);

  class DataHolder : public ThreadSafeRefCounted<DataHolder> {
    WTF_MAKE_NONCOPYABLE(DataHolder);

   public:
    DataHolder();
    ~DataHolder();

    void AllocateNew(unsigned size_in_bytes,
                     SharingType is_shared,
                     InitializationPolicy);
    void Adopt(DataHandle, unsigned size_in_bytes, SharingType is_shared);
    void CopyMemoryFrom(const DataHolder& source);

    const void* Data() const { return data_.Data(); }
    void* Data() { return data_.Data(); }
    unsigned SizeInBytes() const { return size_in_bytes_; }
    bool IsShared() const { return is_shared_ == kShared; }

    void RegisterExternalAllocationWithCurrentContext();
    void UnregisterExternalAllocationWithCurrentContext();

   private:
    void AdjustAmountOfExternalAllocatedMemory(int64_t diff) {
      has_registered_external_allocation_ =
          !has_registered_external_allocation_;
      DCHECK(!diff || (has_registered_external_allocation_ == (diff > 0)));
      CheckIfAdjustAmountOfExternalAllocatedMemoryIsConsistent();
      adjust_amount_of_external_allocated_memory_function_(diff);
    }

    void AdjustAmountOfExternalAllocatedMemory(unsigned diff) {
      AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(diff));
    }

    void CheckIfAdjustAmountOfExternalAllocatedMemoryIsConsistent() {
      DCHECK(adjust_amount_of_external_allocated_memory_function_);

#if DCHECK_IS_ON()
      // Make sure that the function actually used is always the same.
      // Shouldn't be updated during its use.
      if (!last_used_adjust_amount_of_external_allocated_memory_function_) {
        last_used_adjust_amount_of_external_allocated_memory_function_ =
            adjust_amount_of_external_allocated_memory_function_;
      }
      DCHECK_EQ(adjust_amount_of_external_allocated_memory_function_,
                last_used_adjust_amount_of_external_allocated_memory_function_);
#endif
    }

    DataHandle data_;
    unsigned size_in_bytes_;
    SharingType is_shared_;
    bool has_registered_external_allocation_;
  };

  RefPtr<DataHolder> holder_;
  static AdjustAmountOfExternalAllocatedMemoryFunction
      adjust_amount_of_external_allocated_memory_function_;
#if DCHECK_IS_ON()
  static AdjustAmountOfExternalAllocatedMemoryFunction
      last_used_adjust_amount_of_external_allocated_memory_function_;
#endif
};

}  // namespace WTF

#endif  // ArrayBufferContents_h
