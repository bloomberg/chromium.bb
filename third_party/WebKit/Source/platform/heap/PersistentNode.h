// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PersistentNode_h
#define PersistentNode_h

#include <memory>
#include "platform/PlatformExport.h"
#include "platform/heap/ThreadState.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/ThreadingPrimitives.h"

namespace blink {

class CrossThreadPersistentRegion;

class PersistentNode final {
  DISALLOW_NEW();

 public:
  PersistentNode() : self_(nullptr), trace_(nullptr) { DCHECK(IsUnused()); }

#if DCHECK_IS_ON()
  ~PersistentNode() {
    // If you hit this assert, it means that the thread finished
    // without clearing persistent handles that the thread created.
    // We don't enable the assert for the main thread because the
    // main thread finishes without clearing all persistent handles.
    DCHECK(IsMainThread() || IsUnused());
  }
#endif

  // It is dangerous to copy the PersistentNode because it breaks the
  // free list.
  PersistentNode& operator=(const PersistentNode& otherref) = delete;

  // Ideally the trace method should be virtual and automatically dispatch
  // to the most specific implementation. However having a virtual method
  // on PersistentNode leads to too eager template instantiation with MSVC
  // which leads to include cycles.
  // Instead we call the constructor with a TraceCallback which knows the
  // type of the most specific child and calls trace directly. See
  // TraceMethodDelegate in Visitor.h for how this is done.
  void TracePersistentNode(Visitor* visitor) {
    DCHECK(!IsUnused());
    DCHECK(trace_);
    trace_(visitor, self_);
  }

  void Initialize(void* self, TraceCallback trace) {
    DCHECK(IsUnused());
    self_ = self;
    trace_ = trace;
  }

  void SetFreeListNext(PersistentNode* node) {
    DCHECK(!node || node->IsUnused());
    self_ = node;
    trace_ = nullptr;
    DCHECK(IsUnused());
  }

  PersistentNode* FreeListNext() {
    DCHECK(IsUnused());
    PersistentNode* node = reinterpret_cast<PersistentNode*>(self_);
    DCHECK(!node || node->IsUnused());
    return node;
  }

  bool IsUnused() const { return !trace_; }

  void* Self() const { return self_; }

 private:
  // If this PersistentNode is in use:
  //   - m_self points to the corresponding Persistent handle.
  //   - m_trace points to the trace method.
  // If this PersistentNode is freed:
  //   - m_self points to the next freed PersistentNode.
  //   - m_trace is nullptr.
  void* self_;
  TraceCallback trace_;
};

struct PersistentNodeSlots final {
  USING_FAST_MALLOC(PersistentNodeSlots);

 private:
  static const int kSlotCount = 256;
  PersistentNodeSlots* next_;
  PersistentNode slot_[kSlotCount];
  friend class PersistentRegion;
  friend class CrossThreadPersistentRegion;
};

// PersistentRegion provides a region of PersistentNodes. PersistentRegion
// holds a linked list of PersistentNodeSlots, each of which stores
// a predefined number of PersistentNodes. You can call allocatePersistentNode/
// freePersistentNode to allocate/free a PersistentNode on the region.
class PLATFORM_EXPORT PersistentRegion final {
  USING_FAST_MALLOC(PersistentRegion);

 public:
  PersistentRegion()
      : free_list_head_(nullptr),
        slots_(nullptr)
#if DCHECK_IS_ON()
        ,
        persistent_count_(0)
#endif
  {
  }
  ~PersistentRegion();

  PersistentNode* AllocatePersistentNode(void* self, TraceCallback trace) {
#if DCHECK_IS_ON()
    ++persistent_count_;
#endif
    if (UNLIKELY(!free_list_head_))
      EnsurePersistentNodeSlots(self, trace);
    DCHECK(free_list_head_);
    PersistentNode* node = free_list_head_;
    free_list_head_ = free_list_head_->FreeListNext();
    node->Initialize(self, trace);
    DCHECK(!node->IsUnused());
    return node;
  }

  void FreePersistentNode(PersistentNode* persistent_node) {
#if DCHECK_IS_ON()
    DCHECK_GT(persistent_count_, 0);
#endif
    persistent_node->SetFreeListNext(free_list_head_);
    free_list_head_ = persistent_node;
#if DCHECK_IS_ON()
    --persistent_count_;
#endif
  }

  static bool ShouldTracePersistentNode(Visitor*, PersistentNode*) {
    return true;
  }

  void ReleasePersistentNode(PersistentNode*,
                             ThreadState::PersistentClearCallback);
  using ShouldTraceCallback = bool (*)(Visitor*, PersistentNode*);
  void TracePersistentNodes(
      Visitor*,
      ShouldTraceCallback = PersistentRegion::ShouldTracePersistentNode);
  int NumberOfPersistents();

 private:
  friend CrossThreadPersistentRegion;

  void EnsurePersistentNodeSlots(void*, TraceCallback);

  PersistentNode* free_list_head_;
  PersistentNodeSlots* slots_;
#if DCHECK_IS_ON()
  int persistent_count_;
#endif
};

class CrossThreadPersistentRegion final {
  USING_FAST_MALLOC(CrossThreadPersistentRegion);

 public:
  CrossThreadPersistentRegion()
      : persistent_region_(WTF::WrapUnique(new PersistentRegion)) {}

  void AllocatePersistentNode(PersistentNode*& persistent_node,
                              void* self,
                              TraceCallback trace) {
    MutexLocker lock(mutex_);
    PersistentNode* node =
        persistent_region_->AllocatePersistentNode(self, trace);
    ReleaseStore(reinterpret_cast<void* volatile*>(&persistent_node), node);
  }

  void FreePersistentNode(PersistentNode*& persistent_node) {
    MutexLocker lock(mutex_);
    // When the thread that holds the heap object that the cross-thread
    // persistent shuts down, prepareForThreadStateTermination() will clear out
    // the associated CrossThreadPersistent<> and PersistentNode so as to avoid
    // unsafe access. This can overlap with a holder of the
    // CrossThreadPersistent<> also clearing the persistent and freeing the
    // PersistentNode.
    //
    // The lock ensures the updating is ordered, but by the time lock has been
    // acquired the PersistentNode reference may have been cleared out already;
    // check for this.
    if (!persistent_node)
      return;
    persistent_region_->FreePersistentNode(persistent_node);
    ReleaseStore(reinterpret_cast<void* volatile*>(&persistent_node), nullptr);
  }

  class LockScope final {
    STACK_ALLOCATED();

   public:
    LockScope(CrossThreadPersistentRegion& persistent_region,
              bool try_lock = false)
        : persistent_region_(persistent_region), locked_(true) {
      if (try_lock)
        locked_ = persistent_region_.TryLock();
      else
        persistent_region_.lock();
    }
    ~LockScope() {
      if (locked_)
        persistent_region_.unlock();
    }

    // If the lock scope is set up with |try_lock| set to |true|, caller/user
    // is responsible for checking whether the GC lock was taken via
    // |HasLock()|.
    bool HasLock() const { return locked_; }

   private:
    CrossThreadPersistentRegion& persistent_region_;
    bool locked_;
  };

  void TracePersistentNodes(Visitor* visitor) {
// If this assert triggers, you're tracing without being in a LockScope.
#if DCHECK_IS_ON()
    DCHECK(mutex_.Locked());
#endif
    persistent_region_->TracePersistentNodes(
        visitor, CrossThreadPersistentRegion::ShouldTracePersistentNode);
  }

  void PrepareForThreadStateTermination(ThreadState*);

  NO_SANITIZE_ADDRESS
  static bool ShouldTracePersistentNode(Visitor*, PersistentNode*);

#if defined(ADDRESS_SANITIZER)
  void UnpoisonCrossThreadPersistents();
#endif

 private:
  friend class LockScope;

  void lock() { mutex_.lock(); }

  void unlock() { mutex_.unlock(); }

  bool TryLock() { return mutex_.TryLock(); }

  // We don't make CrossThreadPersistentRegion inherit from PersistentRegion
  // because we don't want to virtualize performance-sensitive methods
  // such as PersistentRegion::allocate/freePersistentNode.
  std::unique_ptr<PersistentRegion> persistent_region_;

  // Recursive as prepareForThreadStateTermination() clears a PersistentNode's
  // associated Persistent<> -- it in turn freeing the PersistentNode. And both
  // CrossThreadPersistentRegion operations need a lock on the region before
  // mutating.
  RecursiveMutex mutex_;
};

}  // namespace blink

#endif
