// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_GUARDED_PAGE_ALLOCATOR_H_
#define COMPONENTS_GWP_ASAN_CLIENT_GUARDED_PAGE_ALLOCATOR_H_

#include <atomic>

#include "base/debug/stack_trace.h"
#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/common/allocator_state.h"

namespace gwp_asan {
namespace internal {

// Method to count trailing zero bits in a uint64_t (identical to
// base::bits::CountTrailingZeroBits64 except that it also works on 32-bit
// platforms.)
unsigned CountTrailingZeroBits64(uint64_t x);

// This class encompasses the allocation and deallocation logic on top of the
// AllocatorState. Its members are not inspected or used by the crash handler.
class GWP_ASAN_EXPORT GuardedPageAllocator {
 public:
  // Default maximum alignment for all returned allocations.
  static constexpr size_t kGpaAllocAlignment = 16;

  // Configures this allocator to map memory for num_pages pages (excluding
  // guard pages). num_pages must be in the range [1, kGpaMaxPages]. Init should
  // only be called once.
  void Init(size_t num_pages);

  // On success, returns a pointer to size bytes of page-guarded memory. On
  // failure, returns nullptr. The allocation is not guaranteed to be
  // zero-filled. Failure can occur if memory could not be mapped or protected,
  // or if all guarded pages are already allocated.
  //
  // The align parameter specifies a power of two to align the allocation up to.
  // It must be less than or equal to the allocation size. If it's left as zero
  // it will default to the default alignment the allocator chooses.
  //
  // Precondition: Init() must have been called, align <= size <= page_size_
  void* Allocate(size_t size, size_t align = 0);

  // Deallocates memory pointed to by ptr. ptr must have been previously
  // returned by a call to Allocate.
  void Deallocate(void* ptr);

  // Returns the size requested when ptr was allocated. ptr must have been
  // previously returned by a call to Allocate.
  size_t GetRequestedSize(const void* ptr) const;

  // Get the address of the GuardedPageAllocator crash key (the address of the
  // the shared allocator state with the crash handler.)
  uintptr_t GetCrashKeyAddress() const;

  // Returns true if ptr points to memory managed by this class.
  inline bool PointerIsMine(const void* ptr) const {
    return state_.PointerIsMine(reinterpret_cast<uintptr_t>(ptr));
  }

 private:
  using BitMap = uint64_t;
  static_assert(AllocatorState::kGpaMaxPages == sizeof(BitMap) * 8,
                "Maximum number of pages is the size of free_pages_ bitmap");

  // Does not allocate any memory for the allocator, to finish initializing call
  // Init().
  GuardedPageAllocator();

  // Unmaps memory allocated by this class, if Init was called.
  ~GuardedPageAllocator();

  // Maps pages into memory and sets pages_base_addr_, first_page_addr_, and
  // pages_end_addr on success. Returns true on success, false on failure.
  bool MapPages();

  // Unmaps pages.
  void UnmapPages();

  // Mark page read-write or inaccessible.
  void MarkPageReadWrite(void*);
  void MarkPageInaccessible(void*);

  // Reserves and returns a slot randomly selected from the free slots in
  // free_pages_. Returns SIZE_MAX if no slots available.
  size_t ReserveSlot() LOCKS_EXCLUDED(lock_);

  // Marks the specified slot as unreserved.
  void FreeSlot(size_t slot) LOCKS_EXCLUDED(lock_);

  // Allocate num_pages_ stack traces.
  void AllocateStackTraces();

  // Deallocate stack traces. May only be called after AllocateStackTraces().
  void DeallocateStackTraces();

  // Call the destructor on the allocation and deallocation stack traces for
  // a given slot index if the constructors for those stack traces have been
  // called.
  void DestructStackTrace(size_t slot);

  // Record an allocation or deallocation for a given slot index. This
  // encapsulates the logic for updating the stack traces and metadata for a
  // given slot.
  void RecordAllocationInSlot(size_t slot, size_t size, void* ptr);
  void RecordDeallocationInSlot(size_t slot);

  // Allocator state shared with with the crash analyzer.
  AllocatorState state_;

  // Allocator lock that protects free_pages_.
  base::Lock lock_;

  // Maps each bit to one page.
  // Bit=1: Free. Bit=0: Reserved.
  BitMap free_pages_ GUARDED_BY(lock_) = 0;

  // StackTrace objects for every slot in AllocatorState::data_. We avoid
  // statically allocating the StackTrace objects because they are large and
  // the allocator may be initialized with num_pages_ < kGpaMaxPages.
  base::debug::StackTrace* alloc_traces[AllocatorState::kGpaMaxPages];
  base::debug::StackTrace* dealloc_traces[AllocatorState::kGpaMaxPages];

  // Required for a singleton to access the constructor.
  friend base::NoDestructor<GuardedPageAllocator>;

  DISALLOW_COPY_AND_ASSIGN(GuardedPageAllocator);

  friend class GuardedPageAllocatorTest;
  FRIEND_TEST_ALL_PREFIXES(GuardedPageAllocatorTest,
                           GetNearestValidPageEdgeCases);
  FRIEND_TEST_ALL_PREFIXES(GuardedPageAllocatorTest, GetErrorTypeEdgeCases);
};

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CLIENT_GUARDED_PAGE_ALLOCATOR_H_
