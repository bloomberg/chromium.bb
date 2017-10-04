/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Heap_h
#define Heap_h

#include <memory>
#include "build/build_config.h"
#include "platform/PlatformExport.h"
#include "platform/heap/GCInfo.h"
#include "platform/heap/HeapPage.h"
#include "platform/heap/PageMemory.h"
#include "platform/heap/StackFrameDepth.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/Visitor.h"
#include "platform/wtf/AddressSanitizer.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Atomics.h"
#include "platform/wtf/Forward.h"

namespace blink {

class CallbackStack;
class PagePool;

class PLATFORM_EXPORT HeapAllocHooks {
 public:
  // TODO(hajimehoshi): Pass a type name of the allocated object.
  typedef void AllocationHook(Address, size_t, const char*);
  typedef void FreeHook(Address);

  static void SetAllocationHook(AllocationHook* hook) {
    allocation_hook_ = hook;
  }
  static void SetFreeHook(FreeHook* hook) { free_hook_ = hook; }

  static void AllocationHookIfEnabled(Address address,
                                      size_t size,
                                      const char* type_name) {
    AllocationHook* allocation_hook = allocation_hook_;
    if (UNLIKELY(!!allocation_hook))
      allocation_hook(address, size, type_name);
  }

  static void FreeHookIfEnabled(Address address) {
    FreeHook* free_hook = free_hook_;
    if (UNLIKELY(!!free_hook))
      free_hook(address);
  }

 private:
  static AllocationHook* allocation_hook_;
  static FreeHook* free_hook_;
};

class CrossThreadPersistentRegion;
class HeapCompact;
template <typename T>
class Member;
template <typename T>
class WeakMember;
template <typename T>
class UntracedMember;

template <typename T, bool = NeedsAdjustAndMark<T>::value>
class ObjectAliveTrait;

template <typename T>
class ObjectAliveTrait<T, false> {
  STATIC_ONLY(ObjectAliveTrait);

 public:
  static bool IsHeapObjectAlive(const T* object) {
    static_assert(sizeof(T), "T must be fully defined");
    return HeapObjectHeader::FromPayload(object)->IsMarked();
  }
};

template <typename T>
class ObjectAliveTrait<T, true> {
  STATIC_ONLY(ObjectAliveTrait);

 public:
  NO_SANITIZE_ADDRESS
  static bool IsHeapObjectAlive(const T* object) {
    static_assert(sizeof(T), "T must be fully defined");
    return object->GetHeapObjectHeader()->IsMarked();
  }
};

class PLATFORM_EXPORT ProcessHeap {
  STATIC_ONLY(ProcessHeap);

 public:
  static void Init();

  static CrossThreadPersistentRegion& GetCrossThreadPersistentRegion();

  static void IncreaseTotalAllocatedObjectSize(size_t delta) {
    AtomicAdd(&total_allocated_object_size_, static_cast<long>(delta));
  }
  static void DecreaseTotalAllocatedObjectSize(size_t delta) {
    AtomicSubtract(&total_allocated_object_size_, static_cast<long>(delta));
  }
  static size_t TotalAllocatedObjectSize() {
    return AcquireLoad(&total_allocated_object_size_);
  }
  static void IncreaseTotalMarkedObjectSize(size_t delta) {
    AtomicAdd(&total_marked_object_size_, static_cast<long>(delta));
  }
  static void DecreaseTotalMarkedObjectSize(size_t delta) {
    AtomicSubtract(&total_marked_object_size_, static_cast<long>(delta));
  }
  static size_t TotalMarkedObjectSize() {
    return AcquireLoad(&total_marked_object_size_);
  }
  static void IncreaseTotalAllocatedSpace(size_t delta) {
    AtomicAdd(&total_allocated_space_, static_cast<long>(delta));
  }
  static void DecreaseTotalAllocatedSpace(size_t delta) {
    AtomicSubtract(&total_allocated_space_, static_cast<long>(delta));
  }
  static size_t TotalAllocatedSpace() {
    return AcquireLoad(&total_allocated_space_);
  }
  static void ResetHeapCounters();

 private:
  static size_t total_allocated_space_;
  static size_t total_allocated_object_size_;
  static size_t total_marked_object_size_;

  friend class ThreadState;
};

// Stats for the heap.
class ThreadHeapStats {
  USING_FAST_MALLOC(ThreadHeapStats);

 public:
  ThreadHeapStats();
  void SetMarkedObjectSizeAtLastCompleteSweep(size_t size) {
    marked_object_size_at_last_complete_sweep_ = size;
  }
  size_t MarkedObjectSizeAtLastCompleteSweep() {
    return marked_object_size_at_last_complete_sweep_;
  }
  void IncreaseAllocatedObjectSize(size_t delta);
  void DecreaseAllocatedObjectSize(size_t delta);
  size_t AllocatedObjectSize() { return allocated_object_size_; }
  void IncreaseMarkedObjectSize(size_t delta);
  size_t MarkedObjectSize() { return marked_object_size_; }
  void IncreaseAllocatedSpace(size_t delta);
  void DecreaseAllocatedSpace(size_t delta);
  size_t AllocatedSpace() { return allocated_space_; }
  size_t ObjectSizeAtLastGC() { return object_size_at_last_gc_; }
  void IncreaseWrapperCount(size_t delta) { wrapper_count_ += delta; }
  void DecreaseWrapperCount(size_t delta) { wrapper_count_ -= delta; }
  size_t WrapperCount() { return AcquireLoad(&wrapper_count_); }
  size_t WrapperCountAtLastGC() { return wrapper_count_at_last_gc_; }
  void IncreaseCollectedWrapperCount(size_t delta) {
    collected_wrapper_count_ += delta;
  }
  size_t CollectedWrapperCount() { return collected_wrapper_count_; }
  size_t PartitionAllocSizeAtLastGC() {
    return partition_alloc_size_at_last_gc_;
  }
  void SetEstimatedMarkingTimePerByte(double estimated_marking_time_per_byte) {
    estimated_marking_time_per_byte_ = estimated_marking_time_per_byte;
  }
  double EstimatedMarkingTimePerByte() const {
    return estimated_marking_time_per_byte_;
  }
  double EstimatedMarkingTime();
  void Reset();

 private:
  size_t allocated_space_;
  size_t allocated_object_size_;
  size_t object_size_at_last_gc_;
  size_t marked_object_size_;
  size_t marked_object_size_at_last_complete_sweep_;
  size_t wrapper_count_;
  size_t wrapper_count_at_last_gc_;
  size_t collected_wrapper_count_;
  size_t partition_alloc_size_at_last_gc_;
  double estimated_marking_time_per_byte_;
};

class PLATFORM_EXPORT ThreadHeap {
 public:
  explicit ThreadHeap(ThreadState*);
  ~ThreadHeap();

  // Returns true for main thread's heap.
  // TODO(keishi): Per-thread-heap will return false.
  bool IsMainThreadHeap() { return this == ThreadHeap::MainThreadHeap(); }
  static ThreadHeap* MainThreadHeap() { return main_thread_heap_; }

#if DCHECK_IS_ON()
  BasePage* FindPageFromAddress(Address);
#endif

  template <typename T>
  static inline bool IsHeapObjectAlive(const T* object) {
    static_assert(sizeof(T), "T must be fully defined");
    // The strongification of collections relies on the fact that once a
    // collection has been strongified, there is no way that it can contain
    // non-live entries, so no entries will be removed. Since you can't set
    // the mark bit on a null pointer, that means that null pointers are
    // always 'alive'.
    if (!object)
      return true;
    // TODO(keishi): some tests create CrossThreadPersistent on non attached
    // threads.
    if (!ThreadState::Current())
      return true;
    DCHECK(&ThreadState::Current()->Heap() ==
           &PageFromObject(object)->Arena()->GetThreadState()->Heap());
    return ObjectAliveTrait<T>::IsHeapObjectAlive(object);
  }
  template <typename T>
  static inline bool IsHeapObjectAlive(const Member<T>& member) {
    return IsHeapObjectAlive(member.Get());
  }
  template <typename T>
  static inline bool IsHeapObjectAlive(const WeakMember<T>& member) {
    return IsHeapObjectAlive(member.Get());
  }
  template <typename T>
  static inline bool IsHeapObjectAlive(const UntracedMember<T>& member) {
    return IsHeapObjectAlive(member.Get());
  }

  StackFrameDepth& GetStackFrameDepth() { return stack_frame_depth_; }

  ThreadHeapStats& HeapStats() { return stats_; }
  CallbackStack* MarkingStack() const { return marking_stack_.get(); }
  CallbackStack* PostMarkingCallbackStack() const {
    return post_marking_callback_stack_.get();
  }
  CallbackStack* WeakCallbackStack() const {
    return weak_callback_stack_.get();
  }
  CallbackStack* EphemeronStack() const { return ephemeron_stack_.get(); }

  void VisitPersistentRoots(Visitor*);
  void VisitStackRoots(Visitor*);
  void EnterSafePoint(ThreadState*);
  void LeaveSafePoint();

  // Is the finalizable GC object still alive, but slated for lazy sweeping?
  // If a lazy sweep is in progress, returns true if the object was found
  // to be not reachable during the marking phase, but it has yet to be swept
  // and finalized. The predicate returns false in all other cases.
  //
  // Holding a reference to an already-dead object is not a valid state
  // to be in; willObjectBeLazilySwept() has undefined behavior if passed
  // such a reference.
  template <typename T>
  NO_SANITIZE_ADDRESS static bool WillObjectBeLazilySwept(
      const T* object_pointer) {
    static_assert(IsGarbageCollectedType<T>::value,
                  "only objects deriving from GarbageCollected can be used.");
    BasePage* page = PageFromObject(object_pointer);
    // Page has been swept and it is still alive.
    if (page->HasBeenSwept())
      return false;
    DCHECK(page->Arena()->GetThreadState()->IsSweepingInProgress());

    // If marked and alive, the object hasn't yet been swept..and won't
    // be once its page is processed.
    if (ThreadHeap::IsHeapObjectAlive(const_cast<T*>(object_pointer)))
      return false;

    if (page->IsLargeObjectPage())
      return true;

    // If the object is unmarked, it may be on the page currently being
    // lazily swept.
    return page->Arena()->WillObjectBeLazilySwept(
        page, const_cast<T*>(object_pointer));
  }

  // Push a trace callback on the marking stack.
  void PushTraceCallback(void* container_object, TraceCallback);

  // Push a trace callback on the post-marking callback stack.  These
  // callbacks are called after normal marking (including ephemeron
  // iteration).
  void PushPostMarkingCallback(void*, TraceCallback);

  // Push a weak callback. The weak callback is called when the object
  // doesn't get marked in the current GC.
  void PushWeakCallback(void*, WeakCallback);

  // Pop the top of a marking stack and call the callback with the visitor
  // and the object.  Returns false when there is nothing more to do.
  bool PopAndInvokeTraceCallback(Visitor*);

  // Remove an item from the post-marking callback stack and call
  // the callback with the visitor and the object pointer.  Returns
  // false when there is nothing more to do.
  bool PopAndInvokePostMarkingCallback(Visitor*);

  // Remove an item from the weak callback work list and call the callback
  // with the visitor and the closure pointer.  Returns false when there is
  // nothing more to do.
  bool PopAndInvokeWeakCallback(Visitor*);

  // Register an ephemeron table for fixed-point iteration.
  void RegisterWeakTable(void* container_object,
                         EphemeronCallback,
                         EphemeronCallback);
#if DCHECK_IS_ON()
  bool WeakTableRegistered(const void*);
#endif

  // Heap compaction registration methods:

  // Register |slot| as containing a reference to a movable heap object.
  //
  // When compaction moves the object pointed to by |*slot| to |newAddress|,
  // |*slot| must be updated to hold |newAddress| instead.
  void RegisterMovingObjectReference(MovableReference*);

  // Register a callback to be invoked upon moving the object starting at
  // |reference|; see |MovingObjectCallback| documentation for details.
  //
  // This callback mechanism is needed to account for backing store objects
  // containing intra-object pointers, all of which must be relocated/rebased
  // with respect to the moved-to location.
  //
  // For Blink, |HeapLinkedHashSet<>| is currently the only abstraction which
  // relies on this feature.
  void RegisterMovingObjectCallback(MovableReference,
                                    MovingObjectCallback,
                                    void* callback_data);

  RegionTree* GetRegionTree() { return region_tree_.get(); }

  static inline size_t AllocationSizeFromSize(size_t size) {
    // Add space for header.
    size_t allocation_size = size + sizeof(HeapObjectHeader);
    // The allocation size calculation can overflow for large sizes.
    CHECK_GT(allocation_size, size);
    // Align size with allocation granularity.
    allocation_size = (allocation_size + kAllocationMask) & ~kAllocationMask;
    return allocation_size;
  }
  static Address AllocateOnArenaIndex(ThreadState*,
                                      size_t,
                                      int arena_index,
                                      size_t gc_info_index,
                                      const char* type_name);
  template <typename T>
  static Address Allocate(size_t, bool eagerly_sweep = false);
  template <typename T>
  static Address Reallocate(void* previous, size_t);

  void ProcessMarkingStack(Visitor*);
  void PostMarkingProcessing(Visitor*);
  void WeakProcessing(Visitor*);
  bool AdvanceMarkingStackProcessing(Visitor*, double deadline_seconds);

  // Conservatively checks whether an address is a pointer in any of the
  // thread heaps.  If so marks the object pointed to as live.
  Address CheckAndMarkPointer(Visitor*, Address);
#if DCHECK_IS_ON()
  Address CheckAndMarkPointer(Visitor*,
                              Address,
                              MarkedPointerCallbackForTesting);
#endif

  size_t ObjectPayloadSizeForTesting();

  void FlushHeapDoesNotContainCache();

  PagePool* GetFreePagePool() { return free_page_pool_.get(); }

  // This look-up uses the region search tree and a negative contains cache to
  // provide an efficient mapping from arbitrary addresses to the containing
  // heap-page if one exists.
  BasePage* LookupPageForAddress(Address);

  static const GCInfo* GcInfo(size_t gc_info_index) {
    DCHECK_GE(gc_info_index, 1u);
    DCHECK(gc_info_index < GCInfoTable::kMaxIndex);
    DCHECK(g_gc_info_table);
    const GCInfo* info = g_gc_info_table[gc_info_index];
    DCHECK(info);
    return info;
  }

  static void ReportMemoryUsageHistogram();
  static void ReportMemoryUsageForTracing();

  HeapCompact* Compaction();

 private:
  // Reset counters that track live and allocated-since-last-GC sizes.
  void ResetHeapCounters();

  static int ArenaIndexForObjectSize(size_t);
  static bool IsNormalArenaIndex(int);

  void CommitCallbackStacks();
  void DecommitCallbackStacks();

  ThreadState* thread_state_;
  ThreadHeapStats stats_;
  std::unique_ptr<RegionTree> region_tree_;
  std::unique_ptr<HeapDoesNotContainCache> heap_does_not_contain_cache_;
  std::unique_ptr<PagePool> free_page_pool_;
  std::unique_ptr<CallbackStack> marking_stack_;
  std::unique_ptr<CallbackStack> post_marking_callback_stack_;
  std::unique_ptr<CallbackStack> weak_callback_stack_;
  std::unique_ptr<CallbackStack> ephemeron_stack_;
  StackFrameDepth stack_frame_depth_;

  std::unique_ptr<HeapCompact> compaction_;

  static ThreadHeap* main_thread_heap_;

  friend class ThreadState;
};

template <typename T>
struct IsEagerlyFinalizedType {
  STATIC_ONLY(IsEagerlyFinalizedType);

 private:
  typedef char YesType;
  struct NoType {
    char padding[8];
  };

  template <typename U>
  static YesType CheckMarker(typename U::IsEagerlyFinalizedMarker*);
  template <typename U>
  static NoType CheckMarker(...);

 public:
  static const bool value = sizeof(CheckMarker<T>(nullptr)) == sizeof(YesType);
};

template <typename T>
class GarbageCollected {
  IS_GARBAGE_COLLECTED_TYPE();
  WTF_MAKE_NONCOPYABLE(GarbageCollected);

  // For now direct allocation of arrays on the heap is not allowed.
  void* operator new[](size_t size);

#if defined(OS_WIN) && defined(COMPILER_MSVC)
  // Due to some quirkiness in the MSVC compiler we have to provide
  // the delete[] operator in the GarbageCollected subclasses as it
  // is called when a class is exported in a DLL.
 protected:
  void operator delete[](void* p) { NOTREACHED(); }
#else
  void operator delete[](void* p);
#endif

 public:
  using GarbageCollectedType = T;

  void* operator new(size_t size) {
    return AllocateObject(size, IsEagerlyFinalizedType<T>::value);
  }

  static void* AllocateObject(size_t size, bool eagerly_sweep) {
    return ThreadHeap::Allocate<T>(size, eagerly_sweep);
  }

  void operator delete(void* p) { NOTREACHED(); }

 protected:
  GarbageCollected() {}
};

// Assigning class types to their arenas.
//
// We use sized arenas for most 'normal' objects to improve memory locality.
// It seems that the same type of objects are likely to be accessed together,
// which means that we want to group objects by type. That's one reason
// why we provide dedicated arenas for popular types (e.g., Node, CSSValue),
// but it's not practical to prepare dedicated arenas for all types.
// Thus we group objects by their sizes, hoping that this will approximately
// group objects by their types.
//
// An exception to the use of sized arenas is made for class types that
// require prompt finalization after a garbage collection. That is, their
// instances have to be finalized early and cannot be delayed until lazy
// sweeping kicks in for their heap and page. The EAGERLY_FINALIZE()
// macro is used to declare a class (and its derived classes) as being
// in need of eager finalization. Must be defined with 'public' visibility
// for a class.
//

inline int ThreadHeap::ArenaIndexForObjectSize(size_t size) {
  if (size < 64) {
    if (size < 32)
      return BlinkGC::kNormalPage1ArenaIndex;
    return BlinkGC::kNormalPage2ArenaIndex;
  }
  if (size < 128)
    return BlinkGC::kNormalPage3ArenaIndex;
  return BlinkGC::kNormalPage4ArenaIndex;
}

inline bool ThreadHeap::IsNormalArenaIndex(int index) {
  return index >= BlinkGC::kNormalPage1ArenaIndex &&
         index <= BlinkGC::kNormalPage4ArenaIndex;
}

#define DECLARE_EAGER_FINALIZATION_OPERATOR_NEW() \
 public:                                          \
  GC_PLUGIN_IGNORE("491488")                      \
  void* operator new(size_t size) { return AllocateObject(size, true); }

#define IS_EAGERLY_FINALIZED()                    \
  (PageFromObject(this)->Arena()->ArenaIndex() == \
   BlinkGC::kEagerSweepArenaIndex)
#if DCHECK_IS_ON()
class VerifyEagerFinalization {
  DISALLOW_NEW();

 public:
  ~VerifyEagerFinalization() {
    // If this assert triggers, the class annotated as eagerly
    // finalized ended up not being allocated on the heap
    // set aside for eager finalization. The reason is most
    // likely that the effective 'operator new' overload for
    // this class' leftmost base is for a class that is not
    // eagerly finalized. Declaring and defining an 'operator new'
    // for this class is what's required -- consider using
    // DECLARE_EAGER_FINALIZATION_OPERATOR_NEW().
    DCHECK(IS_EAGERLY_FINALIZED());
  }
};
#define EAGERLY_FINALIZE()                            \
 private:                                             \
  VerifyEagerFinalization verify_eager_finalization_; \
                                                      \
 public:                                              \
  typedef int IsEagerlyFinalizedMarker
#else
#define EAGERLY_FINALIZE() \
 public:                   \
  typedef int IsEagerlyFinalizedMarker
#endif

inline Address ThreadHeap::AllocateOnArenaIndex(ThreadState* state,
                                                size_t size,
                                                int arena_index,
                                                size_t gc_info_index,
                                                const char* type_name) {
  DCHECK(state->IsAllocationAllowed());
  DCHECK_NE(arena_index, BlinkGC::kLargeObjectArenaIndex);
  NormalPageArena* arena =
      static_cast<NormalPageArena*>(state->Arena(arena_index));
  Address address =
      arena->AllocateObject(AllocationSizeFromSize(size), gc_info_index);
  HeapAllocHooks::AllocationHookIfEnabled(address, size, type_name);
  return address;
}

template <typename T>
Address ThreadHeap::Allocate(size_t size, bool eagerly_sweep) {
  ThreadState* state = ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
  const char* type_name = WTF_HEAP_PROFILER_TYPE_NAME(T);
  return ThreadHeap::AllocateOnArenaIndex(
      state, size,
      eagerly_sweep ? BlinkGC::kEagerSweepArenaIndex
                    : ThreadHeap::ArenaIndexForObjectSize(size),
      GCInfoTrait<T>::Index(), type_name);
}

template <typename T>
Address ThreadHeap::Reallocate(void* previous, size_t size) {
  // Not intended to be a full C realloc() substitute;
  // realloc(nullptr, size) is not a supported alias for malloc(size).

  // TODO(sof): promptly free the previous object.
  if (!size) {
    // If the new size is 0 this is considered equivalent to free(previous).
    return nullptr;
  }

  ThreadState* state = ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
  HeapObjectHeader* previous_header = HeapObjectHeader::FromPayload(previous);
  BasePage* page = PageFromObject(previous_header);
  DCHECK(page);

  // Determine arena index of new allocation.
  int arena_index;
  if (size >= kLargeObjectSizeThreshold) {
    arena_index = BlinkGC::kLargeObjectArenaIndex;
  } else {
    arena_index = page->Arena()->ArenaIndex();
    if (IsNormalArenaIndex(arena_index) ||
        arena_index == BlinkGC::kLargeObjectArenaIndex)
      arena_index = ArenaIndexForObjectSize(size);
  }

  size_t gc_info_index = GCInfoTrait<T>::Index();
  // TODO(haraken): We don't support reallocate() for finalizable objects.
  DCHECK(!ThreadHeap::GcInfo(previous_header->GcInfoIndex())->HasFinalizer());
  DCHECK_EQ(previous_header->GcInfoIndex(), gc_info_index);
  HeapAllocHooks::FreeHookIfEnabled(static_cast<Address>(previous));
  Address address;
  if (arena_index == BlinkGC::kLargeObjectArenaIndex) {
    address = page->Arena()->AllocateLargeObject(AllocationSizeFromSize(size),
                                                 gc_info_index);
  } else {
    const char* type_name = WTF_HEAP_PROFILER_TYPE_NAME(T);
    address = ThreadHeap::AllocateOnArenaIndex(state, size, arena_index,
                                               gc_info_index, type_name);
  }
  size_t copy_size = previous_header->PayloadSize();
  if (copy_size > size)
    copy_size = size;
  memcpy(address, previous, copy_size);
  return address;
}

template <typename T>
void Visitor::HandleWeakCell(Visitor* self, void* object) {
  T** cell = reinterpret_cast<T**>(object);
  if (*cell && !ObjectAliveTrait<T>::IsHeapObjectAlive(*cell))
    *cell = nullptr;
}

}  // namespace blink

#include "platform/heap/VisitorImpl.h"

#endif  // Heap_h
