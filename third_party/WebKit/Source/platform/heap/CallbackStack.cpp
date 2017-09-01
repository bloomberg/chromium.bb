// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/CallbackStack.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/allocator/Partitions.h"

namespace blink {

CallbackStackMemoryPool& CallbackStackMemoryPool::Instance() {
  DEFINE_STATIC_LOCAL(CallbackStackMemoryPool, memory_pool, ());
  return memory_pool;
}

void CallbackStackMemoryPool::Initialize() {
  free_list_first_ = 0;
  for (size_t index = 0; index < kPooledBlockCount - 1; ++index) {
    free_list_next_[index] = index + 1;
  }
  free_list_next_[kPooledBlockCount - 1] = -1;
  pooled_memory_ = static_cast<CallbackStack::Item*>(
      WTF::AllocPages(nullptr, kBlockBytes * kPooledBlockCount,
                      WTF::kPageAllocationGranularity, WTF::PageReadWrite));
  CHECK(pooled_memory_);
}

CallbackStack::Item* CallbackStackMemoryPool::Allocate() {
  MutexLocker locker(mutex_);
  // Allocate from a free list if available.
  if (free_list_first_ != -1) {
    size_t index = free_list_first_;
    DCHECK(0 <= index && index < CallbackStackMemoryPool::kPooledBlockCount);
    free_list_first_ = free_list_next_[index];
    free_list_next_[index] = -1;
    return pooled_memory_ + kBlockSize * index;
  }
  // Otherwise, allocate a new memory region.
  CallbackStack::Item* memory =
      static_cast<CallbackStack::Item*>(WTF::Partitions::FastZeroedMalloc(
          kBlockBytes, "CallbackStackMemoryPool"));
  CHECK(memory);
  return memory;
}

void CallbackStackMemoryPool::Free(CallbackStack::Item* memory) {
  MutexLocker locker(mutex_);
  int index = (reinterpret_cast<uintptr_t>(memory) -
               reinterpret_cast<uintptr_t>(pooled_memory_)) /
              (kBlockSize * sizeof(CallbackStack::Item));
  // If the memory is a newly allocated region, free the memory.
  if (index < 0 || static_cast<int>(kPooledBlockCount) <= index) {
    WTF::Partitions::FastFree(memory);
    return;
  }
  // Otherwise, return the memory back to the free list.
  DCHECK_EQ(free_list_next_[index], -1);
  free_list_next_[index] = free_list_first_;
  free_list_first_ = index;
}

CallbackStack::Block::Block(Block* next) {
  buffer_ = CallbackStackMemoryPool::Instance().Allocate();
#if DCHECK_IS_ON()
  Clear();
#endif

  limit_ = &(buffer_[CallbackStackMemoryPool::kBlockSize]);
  current_ = &(buffer_[0]);
  next_ = next;
}

CallbackStack::Block::~Block() {
  CallbackStackMemoryPool::Instance().Free(buffer_);
  buffer_ = nullptr;
  limit_ = nullptr;
  current_ = nullptr;
  next_ = nullptr;
}

#if DCHECK_IS_ON()
void CallbackStack::Block::Clear() {
  for (size_t i = 0; i < CallbackStackMemoryPool::kBlockSize; i++)
    buffer_[i] = Item(0, 0);
}
#endif

void CallbackStack::Block::InvokeEphemeronCallbacks(Visitor* visitor) {
  // This loop can tolerate entries being added by the callbacks after
  // iteration starts.
  for (unsigned i = 0; buffer_ + i < current_; i++) {
    Item& item = buffer_[i];
    item.Call(visitor);
  }
}

#if DCHECK_IS_ON()
bool CallbackStack::Block::HasCallbackForObject(const void* object) {
  for (unsigned i = 0; buffer_ + i < current_; i++) {
    Item* item = &buffer_[i];
    if (item->Object() == object)
      return true;
  }
  return false;
}
#endif

std::unique_ptr<CallbackStack> CallbackStack::Create() {
  return WTF::WrapUnique(new CallbackStack());
}

CallbackStack::CallbackStack() : first_(nullptr), last_(nullptr) {}

CallbackStack::~CallbackStack() {
  CHECK(IsEmpty());
  first_ = nullptr;
  last_ = nullptr;
}

void CallbackStack::Commit() {
  DCHECK(!first_);
  first_ = new Block(first_);
  last_ = first_;
}

void CallbackStack::Decommit() {
  if (!first_)
    return;
  Block* next;
  for (Block* current = first_->Next(); current; current = next) {
    next = current->Next();
    delete current;
  }
  delete first_;
  last_ = first_ = nullptr;
}

bool CallbackStack::IsEmpty() const {
  return !first_ || (HasJustOneBlock() && first_->IsEmptyBlock());
}

CallbackStack::Item* CallbackStack::AllocateEntrySlow() {
  DCHECK(first_);
  DCHECK(!first_->AllocateEntry());
  first_ = new Block(first_);
  return first_->AllocateEntry();
}

CallbackStack::Item* CallbackStack::PopSlow() {
  DCHECK(first_);
  DCHECK(first_->IsEmptyBlock());

  for (;;) {
    Block* next = first_->Next();
    if (!next) {
#if DCHECK_IS_ON()
      first_->Clear();
#endif
      return nullptr;
    }
    delete first_;
    first_ = next;
    if (Item* item = first_->Pop())
      return item;
  }
}

void CallbackStack::InvokeEphemeronCallbacks(Visitor* visitor) {
  // The first block is the only one where new ephemerons are added, so we
  // call the callbacks on that last, to catch any new ephemerons discovered
  // in the callbacks.
  // However, if enough ephemerons were added, we may have a new block that
  // has been prepended to the chain. This will be very rare, but we can
  // handle the situation by starting again and calling all the callbacks
  // on the prepended blocks.
  Block* from = nullptr;
  Block* upto = nullptr;
  while (from != first_) {
    upto = from;
    from = first_;
    InvokeOldestCallbacks(from, upto, visitor);
  }
}

void CallbackStack::InvokeOldestCallbacks(Block* from,
                                          Block* upto,
                                          Visitor* visitor) {
  if (from == upto)
    return;
  DCHECK(from);
  // Recurse first so we get to the newly added entries last.
  InvokeOldestCallbacks(from->Next(), upto, visitor);
  from->InvokeEphemeronCallbacks(visitor);
}

bool CallbackStack::HasJustOneBlock() const {
  DCHECK(first_);
  return !first_->Next();
}

#if DCHECK_IS_ON()
bool CallbackStack::HasCallbackForObject(const void* object) {
  for (Block* current = first_; current; current = current->Next()) {
    if (current->HasCallbackForObject(object))
      return true;
  }
  return false;
}
#endif

}  // namespace blink
