// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The AllocatorState class is the subset of the GuardedPageAllocator that is
// required by the crash handler to analyzer crashes and provide debug
// information. The crash handler initializes an instance of this class from
// the crashed processes memory. Because the out-of-process allocator could be
// corrupted or maliciously tampered with, this class is security sensitive and
// needs to be modified with care. It has been purposefully designed to be:
// - Minimal: This is the minimum set of methods and members required by the
//     crash handler.
// - Trivially copyable: An instance of this object is copied from another
//     processes memory. Ensuring it is trivially copyable means that the crash
//     handler will not accidentally trigger a complex destructor on objects
//     initialized from another processes memory.
// - Free of pointers: Pointers are all uintptr_t since none of these pointers
//     need to be directly dereferenced. Encourage users like the crash handler
//     to consider them addresses instead of pointers.
// - Validatable: The IsValid() method is intended to sanity check the internal
//     fields such that it's safe to call any method on a valid object. All
//     additional methods and fields need to be audited to ensure they maintain
//     this invariant!

#ifndef COMPONENTS_GWP_ASAN_COMMON_ALLOCATOR_STATE_H_
#define COMPONENTS_GWP_ASAN_COMMON_ALLOCATOR_STATE_H_

#include <atomic>

#include "base/debug/stack_trace.h"
#include "base/threading/platform_thread.h"

namespace gwp_asan {
namespace internal {

class GuardedPageAllocator;

class AllocatorState {
 public:
  // Maximum number of pages this class can allocate.
  static constexpr size_t kGpaMaxPages = 64;

  enum class ErrorType {
    kUseAfterFree = 0,
    kBufferUnderflow = 1,
    kBufferOverflow = 2,
    kDoubleFree = 3,
    kUnknown = 4,
  };

  enum class GetMetadataReturnType {
    kUnrelatedCrash = 0,
    kGwpAsanCrash = 1,
    kErrorBadSlot = 2,
  };

  // Structure for storing data about a slot.
  struct SlotMetadata {
    // Information saved for allocations and deallocations.
    struct AllocationInfo {
      // (De)allocation thread id or base::kInvalidThreadId if no (de)allocation
      // occurred.
      base::PlatformThreadId tid = base::kInvalidThreadId;
      // Pointer to stack trace addresses or null if no (de)allocation occurred.
      uintptr_t trace_addr = 0;
      // Stack trace length or 0 if no (de)allocation occurred.
      size_t trace_len = 0;
    };

    // Size of the allocation
    size_t alloc_size = 0;
    // The allocation address.
    uintptr_t alloc_ptr = 0;

    AllocationInfo alloc;
    AllocationInfo dealloc;
  };

  // TODO(vtsyrklevich): Get rid of inline (requires chromium-style plugin
  // update.)
  inline constexpr AllocatorState();

  // Returns true if address is in memory managed by this class.
  inline bool PointerIsMine(uintptr_t addr) const {
    return pages_base_addr <= addr && addr < pages_end_addr;
  }

  // Sanity check allocator internals. This method is used to verify that
  // the allocator base state is well formed when the crash handler analyzes the
  // allocator from a crashing process. This method is security-sensitive, it
  // must validate parameters to ensure that an attacker with the ability to
  // modify the allocator internals can not cause the crash handler to misbehave
  // and cause memory errors.
  bool IsValid() const;

  // This method is meant to be called from the crash handler with a validated
  // AllocatorState object read from the crashed process. This method checks if
  // exception_address is an address in the GWP-ASan region, and writes the
  // error type and slot metadata to the provided arguments if so.
  //
  // Returns an enum indicating an error, unrelated exception, or a GWP-ASan
  // exception (with slot and error_type filled out.)
  GetMetadataReturnType GetMetadataForAddress(uintptr_t exception_address,
                                              SlotMetadata* slot,
                                              ErrorType* error_type) const;

  // Returns the likely error type given an exception address and whether its
  // previously been allocated and deallocated.
  ErrorType GetErrorType(uintptr_t addr,
                         bool allocated,
                         bool deallocated) const;

  // Returns the address of the page that addr resides on.
  uintptr_t GetPageAddr(uintptr_t addr) const;

  // Returns an address somewhere on the valid page nearest to addr.
  uintptr_t GetNearestValidPage(uintptr_t addr) const;

  // Returns the slot number for the page nearest to addr.
  size_t GetNearestSlot(uintptr_t addr) const;

  uintptr_t SlotToAddr(size_t slot) const;
  size_t AddrToSlot(uintptr_t addr) const;

  // Information about every allocation, including its size, offset, and
  // pointers to the allocation/deallocation stack traces (if present.)
  SlotMetadata data[kGpaMaxPages] = {};

  uintptr_t pages_base_addr = 0;  // Points to start of mapped region.
  uintptr_t pages_end_addr = 0;   // Points to the end of mapped region.
  uintptr_t first_page_addr = 0;  // Points to first allocatable page.
  size_t num_pages = 0;  // Number of pages mapped (excluding guard pages).
  size_t page_size = 0;  // Page size.

  // Set to true if a double free has occurred.
  std::atomic<bool> double_free_detected{false};

  DISALLOW_COPY_AND_ASSIGN(AllocatorState);
};

constexpr AllocatorState::AllocatorState() {}

// Ensure that the allocator state is a plain-old-data. That way we can safely
// initialize it by copying memory from out-of-process without worrying about
// destructors operating on the fields in an unexpected way.
static_assert(std::is_trivially_copyable<AllocatorState>(),
              "AllocatorState must be POD");

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_COMMON_ALLOCATOR_STATE_H_
