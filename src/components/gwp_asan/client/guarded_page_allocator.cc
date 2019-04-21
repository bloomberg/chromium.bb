// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/guarded_page_allocator.h"

#include <algorithm>
#include <memory>

#include "base/bits.h"
#include "base/debug/stack_trace.h"
#include "base/no_destructor.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/common/allocator_state.h"
#include "components/gwp_asan/common/crash_key_name.h"
#include "components/gwp_asan/common/pack_stack_trace.h"

#if defined(OS_MACOSX)
#include <pthread.h>
#endif

namespace gwp_asan {
namespace internal {

namespace {

// Report a tid that matches what crashpad collects which may differ from what
// base::PlatformThread::CurrentId() returns.
uint64_t ReportTid() {
#if !defined(OS_MACOSX)
  return base::PlatformThread::CurrentId();
#else
  uint64_t tid = base::kInvalidThreadId;
  pthread_threadid_np(nullptr, &tid);
  return tid;
#endif
}

}  // namespace

// TODO: Delete out-of-line constexpr defininitons once C++17 is in use.
constexpr size_t GuardedPageAllocator::kGpaAllocAlignment;

GuardedPageAllocator::GuardedPageAllocator() {}

void GuardedPageAllocator::Init(size_t max_alloced_pages,
                                size_t num_metadata,
                                size_t total_pages) {
  CHECK_GT(max_alloced_pages, 0U);
  CHECK_LE(max_alloced_pages, num_metadata);
  CHECK_LE(num_metadata, AllocatorState::kMaxMetadata);
  CHECK_LE(num_metadata, total_pages);
  CHECK_LE(total_pages, AllocatorState::kMaxSlots);
  max_alloced_pages_ = max_alloced_pages;
  state_.num_metadata = num_metadata;
  state_.total_pages = total_pages;

  state_.page_size = base::GetPageSize();

  void* region = MapRegion();
  if (!region)
    PLOG(FATAL) << "Failed to reserve allocator region";

  state_.pages_base_addr = reinterpret_cast<uintptr_t>(region);
  state_.first_page_addr = state_.pages_base_addr + state_.page_size;
  state_.pages_end_addr = state_.pages_base_addr + RegionSize();

  {
    // Obtain this lock exclusively to satisfy the thread-safety annotations,
    // there should be no risk of a race here.
    base::AutoLock lock(lock_);

    free_metadata_.resize(state_.num_metadata);
    for (size_t i = 0; i < num_metadata; i++)
      free_metadata_[i] = i;

    free_slots_.resize(total_pages);
    for (size_t i = 0; i < total_pages; i++)
      free_slots_[i] = i;
  }

  slot_to_metadata_idx_.resize(total_pages);
  for (size_t i = 0; i < total_pages; i++)
    slot_to_metadata_idx_[i] = AllocatorState::kInvalidMetadataIdx;
  state_.slot_to_metadata_addr =
      reinterpret_cast<uintptr_t>(&slot_to_metadata_idx_.front());

  metadata_ =
      std::make_unique<AllocatorState::SlotMetadata[]>(state_.num_metadata);
  state_.metadata_addr = reinterpret_cast<uintptr_t>(metadata_.get());
}

GuardedPageAllocator::~GuardedPageAllocator() {
  if (state_.total_pages)
    UnmapRegion();
}

void* GuardedPageAllocator::Allocate(size_t size, size_t align) {
  if (!size || size > state_.page_size || align > state_.page_size)
    return nullptr;

  // Default alignment is size's next smallest power-of-two, up to
  // kGpaAllocAlignment.
  if (!align) {
    align =
        std::min(size_t{1} << base::bits::Log2Floor(size), kGpaAllocAlignment);
  }
  CHECK(base::bits::IsPowerOfTwo(align));

  AllocatorState::SlotIdx free_slot;
  AllocatorState::MetadataIdx free_metadata;
  if (!ReserveSlotAndMetadata(&free_slot, &free_metadata))
    return nullptr;

  uintptr_t free_page = state_.SlotToAddr(free_slot);
  MarkPageReadWrite(reinterpret_cast<void*>(free_page));

  size_t offset;
  if (free_slot & 1)
    // Return right-aligned allocation to detect overflows.
    offset = state_.page_size - base::bits::Align(size, align);
  else
    // Return left-aligned allocation to detect underflows.
    offset = 0;

  void* alloc = reinterpret_cast<void*>(free_page + offset);

  // Initialize slot metadata and only then update slot_to_metadata_idx so that
  // the mapping never points to an incorrect metadata mapping.
  RecordAllocationMetadata(free_metadata, size, alloc);
  {
    // Lock to avoid race with the slot_to_metadata_idx_ check/write in
    // ReserveSlotAndMetadata().
    base::AutoLock lock(lock_);
    slot_to_metadata_idx_[free_slot] = free_metadata;
  }

  return alloc;
}

void GuardedPageAllocator::Deallocate(void* ptr) {
  CHECK(PointerIsMine(ptr));

  const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  AllocatorState::SlotIdx slot = state_.AddrToSlot(state_.GetPageAddr(addr));
  AllocatorState::MetadataIdx metadata_idx = slot_to_metadata_idx_[slot];

  // Check for a call to free() with an incorrect pointer, e.g. the pointer does
  // not match the allocated pointer. This may occur with a bad free pointer or
  // an outdated double free when the metadata has expired.
  if (metadata_idx == AllocatorState::kInvalidMetadataIdx ||
      addr != metadata_[metadata_idx].alloc_ptr) {
    state_.free_invalid_address = addr;
    __builtin_trap();
  }

  // Check for double free.
  if (metadata_[metadata_idx].deallocation_occurred.exchange(true)) {
    state_.double_free_address = addr;
    // TODO(https://crbug.com/925447): The other thread may not be done writing
    // a stack trace so we could spin here until it's read; however, it's also
    // possible we are racing an allocation in the middle of
    // RecordAllocationMetadata. For now it's possible a racy double free could
    // lead to a bad stack trace, but no internal allocator corruption.
    __builtin_trap();
  }

  // Record deallocation stack trace/thread id before marking the page
  // inaccessible in case a use-after-free occurs immediately.
  RecordDeallocationMetadata(metadata_idx);
  MarkPageInaccessible(reinterpret_cast<void*>(state_.GetPageAddr(addr)));

  FreeSlotAndMetadata(slot, metadata_idx);
}

size_t GuardedPageAllocator::GetRequestedSize(const void* ptr) const {
  CHECK(PointerIsMine(ptr));
  const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  AllocatorState::SlotIdx slot = state_.AddrToSlot(state_.GetPageAddr(addr));
  AllocatorState::MetadataIdx metadata_idx = slot_to_metadata_idx_[slot];
#if !defined(OS_MACOSX)
  CHECK_LT(metadata_idx, state_.num_metadata);
  CHECK_EQ(addr, metadata_[metadata_idx].alloc_ptr);
#else
  // macOS core libraries call malloc_size() inside an allocation. The macOS
  // malloc_size() returns 0 when the pointer is not recognized.
  // https://crbug.com/946736
  if (metadata_idx == AllocatorState::kInvalidMetadataIdx ||
      addr != metadata_[metadata_idx].alloc_ptr)
    return 0;
#endif
  return metadata_[metadata_idx].alloc_size;
}

size_t GuardedPageAllocator::RegionSize() const {
  return (2 * state_.total_pages + 1) * state_.page_size;
}

bool GuardedPageAllocator::ReserveSlotAndMetadata(
    AllocatorState::SlotIdx* slot,
    AllocatorState::MetadataIdx* metadata_idx) {
  base::AutoLock lock(lock_);
  if (num_alloced_pages_ == max_alloced_pages_)
    return false;
  num_alloced_pages_++;

  DCHECK(!free_metadata_.empty());
  DCHECK_LE(free_metadata_.size(), state_.num_metadata);
  size_t rand = base::RandGenerator(free_metadata_.size());
  *metadata_idx = free_metadata_[rand];
  free_metadata_[rand] = free_metadata_.back();
  free_metadata_.pop_back();
  DCHECK_EQ(free_metadata_.size(), state_.num_metadata - num_alloced_pages_);

  // If this metadata has been previously used, overwrite the outdated
  // slot_to_metadata_idx mapping from the previous use if it's still valid.
  if (metadata_[*metadata_idx].alloc_ptr) {
    DCHECK(state_.PointerIsMine(metadata_[*metadata_idx].alloc_ptr));
    size_t old_slot = state_.GetNearestSlot(metadata_[*metadata_idx].alloc_ptr);
    if (slot_to_metadata_idx_[old_slot] == *metadata_idx)
      slot_to_metadata_idx_[old_slot] = AllocatorState::kInvalidMetadataIdx;
  }

  DCHECK(!free_slots_.empty());
  DCHECK_LE(free_slots_.size(), state_.total_pages);
  rand = base::RandGenerator(free_slots_.size());
  *slot = free_slots_[rand];
  free_slots_[rand] = free_slots_.back();
  free_slots_.pop_back();
  DCHECK_EQ(free_slots_.size(), state_.total_pages - num_alloced_pages_);

  return true;
}

void GuardedPageAllocator::FreeSlotAndMetadata(
    AllocatorState::SlotIdx slot,
    AllocatorState::MetadataIdx metadata_idx) {
  DCHECK_LT(slot, state_.total_pages);
  DCHECK_LT(metadata_idx, state_.num_metadata);

  base::AutoLock lock(lock_);
  DCHECK_LT(free_slots_.size(), state_.total_pages);
  free_slots_.push_back(slot);

  DCHECK_LT(free_metadata_.size(), state_.total_pages);
  free_metadata_.push_back(metadata_idx);

  DCHECK_GT(num_alloced_pages_, 0U);
  num_alloced_pages_--;

  DCHECK_EQ(free_metadata_.size(), state_.num_metadata - num_alloced_pages_);
  DCHECK_EQ(free_slots_.size(), state_.total_pages - num_alloced_pages_);
}

void GuardedPageAllocator::RecordAllocationMetadata(
    AllocatorState::MetadataIdx metadata_idx,
    size_t size,
    void* ptr) {
  metadata_[metadata_idx].alloc_size = size;
  metadata_[metadata_idx].alloc_ptr = reinterpret_cast<uintptr_t>(ptr);

  void* trace[AllocatorState::kMaxStackFrames];
  size_t len =
      base::debug::CollectStackTrace(trace, AllocatorState::kMaxStackFrames);
  metadata_[metadata_idx].alloc.trace_len =
      Pack(reinterpret_cast<uintptr_t*>(trace), len,
           metadata_[metadata_idx].stack_trace_pool,
           sizeof(metadata_[metadata_idx].stack_trace_pool) / 2);
  metadata_[metadata_idx].alloc.tid = ReportTid();
  metadata_[metadata_idx].alloc.trace_collected = true;

  metadata_[metadata_idx].dealloc.tid = base::kInvalidThreadId;
  metadata_[metadata_idx].dealloc.trace_len = 0;
  metadata_[metadata_idx].dealloc.trace_collected = false;
  metadata_[metadata_idx].deallocation_occurred = false;
}

void GuardedPageAllocator::RecordDeallocationMetadata(
    AllocatorState::MetadataIdx metadata_idx) {
  void* trace[AllocatorState::kMaxStackFrames];
  size_t len =
      base::debug::CollectStackTrace(trace, AllocatorState::kMaxStackFrames);
  metadata_[metadata_idx].dealloc.trace_len =
      Pack(reinterpret_cast<uintptr_t*>(trace), len,
           metadata_[metadata_idx].stack_trace_pool +
               metadata_[metadata_idx].alloc.trace_len,
           sizeof(metadata_[metadata_idx].stack_trace_pool) -
               metadata_[metadata_idx].alloc.trace_len);
  metadata_[metadata_idx].dealloc.tid = ReportTid();
  metadata_[metadata_idx].dealloc.trace_collected = true;
}

uintptr_t GuardedPageAllocator::GetCrashKeyAddress() const {
  return reinterpret_cast<uintptr_t>(&state_);
}

}  // namespace internal
}  // namespace gwp_asan
