// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"
#include "third_party/blink/renderer/platform/heap/heap_terminated_array.h"
#include "third_party/blink/renderer/platform/heap/heap_terminated_array_builder.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

#if BUILDFLAG(BLINK_HEAP_INCREMENTAL_MARKING)

namespace blink {
namespace incremental_marking_test {

// Visitor that expects every directly reachable object from a given backing
// store to be in the set of provided objects.
template <typename T>
class BackingVisitor : public Visitor {
 public:
  explicit BackingVisitor(ThreadState* state, std::vector<T*>* objects)
      : Visitor(state), objects_(objects) {}
  virtual ~BackingVisitor() {}

  void ProcessBackingStore(HeapObjectHeader* header) {
    EXPECT_TRUE(header->IsValid());
    EXPECT_TRUE(header->IsMarked());
    header->Unmark();
    ThreadHeap::GcInfo(header->GcInfoIndex())->trace_(this, header->Payload());
  }

  void Visit(void* obj, TraceDescriptor desc) final {
    EXPECT_TRUE(obj);
    auto pos = std::find(objects_->begin(), objects_->end(), obj);
    if (objects_->end() != pos)
      objects_->erase(pos);
    // The garbage collector will find those objects so we can mark them.
    HeapObjectHeader* const header =
        HeapObjectHeader::FromPayload(desc.base_object_payload);
    if (!header->IsMarked())
      header->Mark();
  }

  // Unused overrides.
  void VisitWeak(void* object,
                 void** object_slot,
                 TraceDescriptor desc,
                 WeakCallback callback) final {}
  void VisitBackingStoreStrongly(void* object,
                                 void** object_slot,
                                 TraceDescriptor desc) final {}
  void VisitBackingStoreWeakly(void*,
                               void**,
                               TraceDescriptor,
                               WeakCallback,
                               void*) final {}
  void VisitBackingStoreOnly(void*, void**) final {}
  void RegisterBackingStoreCallback(void* backing_store,
                                    MovingObjectCallback,
                                    void* callback_data) final {}
  void RegisterWeakCallback(void* closure, WeakCallback) final {}

 private:
  std::vector<T*>* objects_;
};

// Base class for initializing worklists.
class IncrementalMarkingScopeBase {
 public:
  IncrementalMarkingScopeBase(ThreadState* thread_state)
      : thread_state_(thread_state), heap_(thread_state_->Heap()) {
    heap_.CommitCallbackStacks();
  }

  ~IncrementalMarkingScopeBase() { heap_.DecommitCallbackStacks(); }

  ThreadHeap& heap() const { return heap_; }

 protected:
  ThreadState* const thread_state_;
  ThreadHeap& heap_;
};

class IncrementalMarkingScope : public IncrementalMarkingScopeBase {
 public:
  explicit IncrementalMarkingScope(ThreadState* thread_state)
      : IncrementalMarkingScopeBase(thread_state),
        gc_forbidden_scope_(thread_state),
        marking_worklist_(heap_.GetMarkingWorklist()),
        not_fully_constructed_worklist_(
            heap_.GetNotFullyConstructedWorklist()) {
    thread_state_->SetGCPhase(ThreadState::GCPhase::kMarking);
    ThreadState::AtomicPauseScope atomic_pause_scope_(thread_state_);
    EXPECT_TRUE(marking_worklist_->IsGlobalEmpty());
    EXPECT_TRUE(not_fully_constructed_worklist_->IsGlobalEmpty());
    thread_state->EnableIncrementalMarkingBarrier();
    thread_state->current_gc_data_.visitor =
        MarkingVisitor::Create(thread_state, MarkingVisitor::kGlobalMarking);
  }

  ~IncrementalMarkingScope() {
    EXPECT_TRUE(marking_worklist_->IsGlobalEmpty());
    EXPECT_TRUE(not_fully_constructed_worklist_->IsGlobalEmpty());
    thread_state_->DisableIncrementalMarkingBarrier();
    // Need to clear out unused worklists that might have been polluted during
    // test.
    heap_.GetWeakCallbackWorklist()->Clear();
    thread_state_->SetGCPhase(ThreadState::GCPhase::kSweeping);
    thread_state_->SetGCPhase(ThreadState::GCPhase::kNone);
  }

  MarkingWorklist* marking_worklist() const { return marking_worklist_; }
  NotFullyConstructedWorklist* not_fully_constructed_worklist() const {
    return not_fully_constructed_worklist_;
  }

 protected:
  ThreadState::GCForbiddenScope gc_forbidden_scope_;
  MarkingWorklist* const marking_worklist_;
  NotFullyConstructedWorklist* const not_fully_constructed_worklist_;
};

// Expects that the write barrier fires for the objects passed to the
// constructor. This requires that the objects are added to the marking stack
// as well as headers being marked.
template <typename T>
class ExpectWriteBarrierFires : public IncrementalMarkingScope {
 public:
  ExpectWriteBarrierFires(ThreadState* thread_state,
                          std::initializer_list<T*> objects)
      : IncrementalMarkingScope(thread_state), objects_(objects) {
    EXPECT_TRUE(marking_worklist_->IsGlobalEmpty());
    for (T* object : objects_) {
      // Ensure that the object is in the normal arena so we can ignore backing
      // objects on the marking stack.
      CHECK(ThreadHeap::IsNormalArenaIndex(
          PageFromObject(object)->Arena()->ArenaIndex()));
      headers_.push_back(HeapObjectHeader::FromPayload(object));
      EXPECT_FALSE(headers_.back()->IsMarked());
    }
    EXPECT_FALSE(objects_.empty());
  }

  ~ExpectWriteBarrierFires() {
    EXPECT_FALSE(marking_worklist_->IsGlobalEmpty());
    MarkingItem item;
    // All objects watched should be on the marking stack.
    while (marking_worklist_->Pop(WorklistTaskId::MainThread, &item)) {
      T* obj = reinterpret_cast<T*>(item.object);
      // Inspect backing stores to allow specifying objects that are only
      // reachable through a backing store.
      if (!ThreadHeap::IsNormalArenaIndex(
              PageFromObject(obj)->Arena()->ArenaIndex())) {
        BackingVisitor<T> visitor(thread_state_, &objects_);
        visitor.ProcessBackingStore(HeapObjectHeader::FromPayload(obj));
        continue;
      }
      auto pos = std::find(objects_.begin(), objects_.end(), obj);
      if (objects_.end() != pos)
        objects_.erase(pos);
    }
    EXPECT_TRUE(objects_.empty());
    // All headers of objects watched should be marked at this point.
    for (HeapObjectHeader* header : headers_) {
      EXPECT_TRUE(header->IsMarked());
      header->Unmark();
    }
    EXPECT_TRUE(marking_worklist_->IsGlobalEmpty());
  }

 private:
  std::vector<T*> objects_;
  std::vector<HeapObjectHeader*> headers_;
};

// Expects that no write barrier fires for the objects passed to the
// constructor. This requires that the marking stack stays empty and the marking
// state of the object stays the same across the lifetime of the scope.
template <typename T>
class ExpectNoWriteBarrierFires : public IncrementalMarkingScope {
 public:
  ExpectNoWriteBarrierFires(ThreadState* thread_state,
                            std::initializer_list<T*> objects)
      : IncrementalMarkingScope(thread_state), objects_(objects) {
    EXPECT_TRUE(marking_worklist_->IsGlobalEmpty());
    for (T* object : objects_) {
      HeapObjectHeader* header = HeapObjectHeader::FromPayload(object);
      headers_.push_back(header);
      was_marked_.push_back(header->IsMarked());
    }
  }

  ~ExpectNoWriteBarrierFires() {
    EXPECT_TRUE(marking_worklist_->IsGlobalEmpty());
    for (size_t i = 0; i < headers_.size(); i++) {
      EXPECT_EQ(was_marked_[i], headers_[i]->IsMarked());
      headers_[i]->Unmark();
    }
  }

 private:
  std::vector<T*> objects_;
  std::vector<HeapObjectHeader*> headers_;
  std::vector<bool> was_marked_;
};

class Object : public GarbageCollected<Object> {
 public:
  static Object* Create() { return new Object(); }
  static Object* Create(Object* next) { return new Object(next); }

  void set_next(Object* next) { next_ = next; }

  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(next_); }

  bool IsMarked() const {
    return HeapObjectHeader::FromPayload(this)->IsMarked();
  }

 private:
  Object() : next_(nullptr) {}
  explicit Object(Object* next) : next_(next) {}

  Member<Object> next_;
};

// =============================================================================
// Basic infrastructure support. ===============================================
// =============================================================================

TEST(IncrementalMarkingTest, EnableDisableBarrier) {
  EXPECT_FALSE(ThreadState::Current()->IsIncrementalMarking());
  ThreadState::Current()->EnableIncrementalMarkingBarrier();
  EXPECT_TRUE(ThreadState::Current()->IsIncrementalMarking());
  EXPECT_TRUE(ThreadState::IsAnyIncrementalMarking());
  ThreadState::Current()->DisableIncrementalMarkingBarrier();
  EXPECT_FALSE(ThreadState::Current()->IsIncrementalMarking());
}

TEST(IncrementalMarkingTest, StackFrameDepthDisabled) {
  IncrementalMarkingScope scope(ThreadState::Current());
  EXPECT_FALSE(scope.heap().GetStackFrameDepth().IsSafeToRecurse());
}

TEST(IncrementalMarkingTest, ManualWriteBarrierTriggersWhenMarkingIsOn) {
  Object* object = Object::Create();
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {object});
    EXPECT_FALSE(object->IsMarked());
    MarkingVisitor::WriteBarrier(object);
    EXPECT_TRUE(object->IsMarked());
  }
}

TEST(IncrementalMarkingTest, ManualWriteBarrierBailoutWhenMarkingIsOff) {
  Object* object = Object::Create();
  EXPECT_FALSE(object->IsMarked());
  MarkingVisitor::WriteBarrier(object);
  EXPECT_FALSE(object->IsMarked());
}

// =============================================================================
// Member<T> support. ==========================================================
// =============================================================================

TEST(IncrementalMarkingTest, MemberSetUnmarkedObject) {
  Object* parent = Object::Create();
  Object* child = Object::Create();
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {child});
    EXPECT_FALSE(child->IsMarked());
    parent->set_next(child);
    EXPECT_TRUE(child->IsMarked());
  }
}

TEST(IncrementalMarkingTest, MemberSetMarkedObjectNoBarrier) {
  Object* parent = Object::Create();
  Object* child = Object::Create();
  HeapObjectHeader::FromPayload(child)->Mark();
  {
    ExpectNoWriteBarrierFires<Object> scope(ThreadState::Current(), {child});
    parent->set_next(child);
  }
}

TEST(IncrementalMarkingTest, MemberInitializingStoreNoBarrier) {
  Object* object1 = Object::Create();
  HeapObjectHeader* object1_header = HeapObjectHeader::FromPayload(object1);
  {
    IncrementalMarkingScope scope(ThreadState::Current());
    EXPECT_FALSE(object1_header->IsMarked());
    Object* object2 = Object::Create(object1);
    HeapObjectHeader* object2_header = HeapObjectHeader::FromPayload(object2);
    EXPECT_FALSE(object1_header->IsMarked());
    EXPECT_FALSE(object2_header->IsMarked());
  }
}

TEST(IncrementalMarkingTest, MemberReferenceAssignMember) {
  Object* obj = Object::Create();
  Member<Object> m1;
  Member<Object>& m2 = m1;
  Member<Object> m3(obj);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    m2 = m3;
  }
}

TEST(IncrementalMarkingTest, MemberSetDeletedValueNoBarrier) {
  Member<Object> m;
  {
    ExpectNoWriteBarrierFires<Object> scope(ThreadState::Current(), {});
    m = WTF::kHashTableDeletedValue;
  }
}

TEST(IncrementalMarkingTest, MemberCopyDeletedValueNoBarrier) {
  Member<Object> m1(WTF::kHashTableDeletedValue);
  {
    ExpectNoWriteBarrierFires<Object> scope(ThreadState::Current(), {});
    Member<Object> m2(m1);
  }
}

TEST(IncrementalMarkingTest, MemberHashTraitConstructDeletedValueNoBarrier) {
  Member<Object> m1;
  {
    ExpectNoWriteBarrierFires<Object> scope(ThreadState::Current(), {});
    HashTraits<Member<Object>>::ConstructDeletedValue(m1, false);
  }
}

TEST(IncrementalMarkingTest, MemberHashTraitIsDeletedValueNoBarrier) {
  Member<Object> m1(Object::Create());
  {
    ExpectNoWriteBarrierFires<Object> scope(ThreadState::Current(), {});
    EXPECT_FALSE(HashTraits<Member<Object>>::IsDeletedValue(m1));
  }
}

// =============================================================================
// Mixin support. ==============================================================
// =============================================================================

namespace {

class Mixin : public GarbageCollectedMixin {
 public:
  Mixin() : next_(nullptr) {}
  virtual ~Mixin() {}

  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(next_); }

  virtual void Bar() {}

 protected:
  Member<Object> next_;
};

class ClassWithVirtual {
 protected:
  virtual void Foo() {}
};

class Child : public GarbageCollected<Child>,
              public ClassWithVirtual,
              public Mixin {
  USING_GARBAGE_COLLECTED_MIXIN(Child);

 public:
  static Child* Create() { return new Child(); }
  virtual ~Child() {}

  virtual void Trace(blink::Visitor* visitor) { Mixin::Trace(visitor); }

  virtual void Foo() {}
  virtual void Bar() {}

 protected:
  Child() : ClassWithVirtual(), Mixin() {}
};

class ParentWithMixinPointer : public GarbageCollected<ParentWithMixinPointer> {
 public:
  static ParentWithMixinPointer* Create() {
    return new ParentWithMixinPointer();
  }

  void set_mixin(Mixin* mixin) { mixin_ = mixin; }

  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(mixin_); }

 protected:
  ParentWithMixinPointer() : mixin_(nullptr) {}

  Member<Mixin> mixin_;
};

}  // namespace

TEST(IncrementalMarkingTest, WriteBarrierOnUnmarkedMixinApplication) {
  ParentWithMixinPointer* parent = ParentWithMixinPointer::Create();
  Child* child = Child::Create();
  Mixin* mixin = static_cast<Mixin*>(child);
  EXPECT_NE(static_cast<void*>(child), static_cast<void*>(mixin));
  {
    ExpectWriteBarrierFires<Child> scope(ThreadState::Current(), {child});
    parent->set_mixin(mixin);
  }
}

TEST(IncrementalMarkingTest, NoWriteBarrierOnMarkedMixinApplication) {
  ParentWithMixinPointer* parent = ParentWithMixinPointer::Create();
  Child* child = Child::Create();
  HeapObjectHeader::FromPayload(child)->Mark();
  Mixin* mixin = static_cast<Mixin*>(child);
  EXPECT_NE(static_cast<void*>(child), static_cast<void*>(mixin));
  {
    ExpectNoWriteBarrierFires<Child> scope(ThreadState::Current(), {child});
    parent->set_mixin(mixin);
  }
}

// =============================================================================
// HeapVector support. =========================================================
// =============================================================================

namespace {

// HeapVector allows for insertion of container objects that can be traced but
// are themselves non-garbage collected.
class NonGarbageCollectedContainer {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  NonGarbageCollectedContainer(Object* obj, int y) : obj_(obj), y_(y) {}

  virtual ~NonGarbageCollectedContainer() {}
  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(obj_); }

 private:
  Member<Object> obj_;
  int y_;
};

class NonGarbageCollectedContainerRoot {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  NonGarbageCollectedContainerRoot(Object* obj1, Object* obj2, int y)
      : next_(obj1, y), obj_(obj2) {}
  virtual ~NonGarbageCollectedContainerRoot() {}

  virtual void Trace(blink::Visitor* visitor) {
    visitor->Trace(next_);
    visitor->Trace(obj_);
  }

 private:
  NonGarbageCollectedContainer next_;
  Member<Object> obj_;
};

}  // namespace

TEST(IncrementalMarkingTest, HeapVectorPushBackMember) {
  Object* obj = Object::Create();
  HeapVector<Member<Object>> vec;
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    vec.push_back(obj);
  }
}

TEST(IncrementalMarkingTest, HeapVectorPushBackNonGCedContainer) {
  Object* obj = Object::Create();
  HeapVector<NonGarbageCollectedContainer> vec;
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    vec.push_back(NonGarbageCollectedContainer(obj, 1));
  }
}

TEST(IncrementalMarkingTest, HeapVectorPushBackStdPair) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapVector<std::pair<Member<Object>, Member<Object>>> vec;
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    vec.push_back(std::make_pair(Member<Object>(obj1), Member<Object>(obj2)));
  }
}

TEST(IncrementalMarkingTest, HeapVectorEmplaceBackMember) {
  Object* obj = Object::Create();
  HeapVector<Member<Object>> vec;
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    vec.emplace_back(obj);
  }
}

TEST(IncrementalMarkingTest, HeapVectorEmplaceBackNonGCedContainer) {
  Object* obj = Object::Create();
  HeapVector<NonGarbageCollectedContainer> vec;
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    vec.emplace_back(obj, 1);
  }
}

TEST(IncrementalMarkingTest, HeapVectorEmplaceBackStdPair) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapVector<std::pair<Member<Object>, Member<Object>>> vec;
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    vec.emplace_back(obj1, obj2);
  }
}

TEST(IncrementalMarkingTest, HeapVectorCopyMember) {
  Object* object = Object::Create();
  HeapVector<Member<Object>> vec1;
  vec1.push_back(object);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {object});
    HeapVector<Member<Object>> vec2(vec1);
  }
}

TEST(IncrementalMarkingTest, HeapVectorCopyNonGCedContainer) {
  Object* obj = Object::Create();
  HeapVector<NonGarbageCollectedContainer> vec1;
  vec1.emplace_back(obj, 1);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    HeapVector<NonGarbageCollectedContainer> vec2(vec1);
  }
}

TEST(IncrementalMarkingTest, HeapVectorCopyStdPair) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapVector<std::pair<Member<Object>, Member<Object>>> vec1;
  vec1.emplace_back(obj1, obj2);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    HeapVector<std::pair<Member<Object>, Member<Object>>> vec2(vec1);
  }
}

TEST(IncrementalMarkingTest, HeapVectorMoveMember) {
  Object* obj = Object::Create();
  HeapVector<Member<Object>> vec1;
  vec1.push_back(obj);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    HeapVector<Member<Object>> vec2(std::move(vec1));
  }
}

TEST(IncrementalMarkingTest, HeapVectorMoveNonGCedContainer) {
  Object* obj = Object::Create();
  HeapVector<NonGarbageCollectedContainer> vec1;
  vec1.emplace_back(obj, 1);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    HeapVector<NonGarbageCollectedContainer> vec2(std::move(vec1));
  }
}

TEST(IncrementalMarkingTest, HeapVectorMoveStdPair) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapVector<std::pair<Member<Object>, Member<Object>>> vec1;
  vec1.emplace_back(obj1, obj2);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    HeapVector<std::pair<Member<Object>, Member<Object>>> vec2(std::move(vec1));
  }
}

TEST(IncrementalMarkingTest, HeapVectorSwapMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapVector<Member<Object>> vec1;
  vec1.push_back(obj1);
  HeapVector<Member<Object>> vec2;
  vec2.push_back(obj2);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    std::swap(vec1, vec2);
  }
}

TEST(IncrementalMarkingTest, HeapVectorSwapNonGCedContainer) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapVector<NonGarbageCollectedContainer> vec1;
  vec1.emplace_back(obj1, 1);
  HeapVector<NonGarbageCollectedContainer> vec2;
  vec2.emplace_back(obj2, 2);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    std::swap(vec1, vec2);
  }
}

TEST(IncrementalMarkingTest, HeapVectorSwapStdPair) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapVector<std::pair<Member<Object>, Member<Object>>> vec1;
  vec1.emplace_back(obj1, nullptr);
  HeapVector<std::pair<Member<Object>, Member<Object>>> vec2;
  vec2.emplace_back(nullptr, obj2);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    std::swap(vec1, vec2);
  }
}

TEST(IncrementalMarkingTest, HeapVectorSubscriptOperator) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapVector<Member<Object>> vec;
  vec.push_back(obj1);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj2});
    EXPECT_EQ(1u, vec.size());
    EXPECT_EQ(obj1, vec[0]);
    vec[0] = obj2;
    EXPECT_EQ(obj2, vec[0]);
    EXPECT_FALSE(obj1->IsMarked());
  }
}

TEST(IncrementalMarkingTest, HeapVectorEagerTracingStopsAtMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  Object* obj3 = Object::Create();
  obj1->set_next(obj3);
  HeapVector<NonGarbageCollectedContainerRoot> vec;
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    vec.emplace_back(obj1, obj2, 3);
    // |obj3| is only reachable from |obj1| which is not eagerly traced. Only
    // objects without object headers are eagerly traced.
    EXPECT_FALSE(obj3->IsMarked());
  }
}

// =============================================================================
// HeapDoublyLinkedList support. ===============================================
// =============================================================================

namespace {

class ObjectNode : public GarbageCollected<ObjectNode>,
                   public DoublyLinkedListNode<ObjectNode> {
 public:
  explicit ObjectNode(Object* obj) : obj_(obj) {}

  void Trace(Visitor* visitor) {
    visitor->Trace(obj_);
    visitor->Trace(prev_);
    visitor->Trace(next_);
  }

 private:
  friend class WTF::DoublyLinkedListNode<ObjectNode>;

  Member<Object> obj_;
  Member<ObjectNode> prev_;
  Member<ObjectNode> next_;
};

}  // namespace

TEST(IncrementalMarkingTest, HeapDoublyLinkedListPush) {
  Object* obj = Object::Create();
  ObjectNode* obj_node = new ObjectNode(obj);
  HeapDoublyLinkedList<ObjectNode> list;
  {
    ExpectWriteBarrierFires<ObjectNode> scope(ThreadState::Current(),
                                              {obj_node});
    list.Push(obj_node);
    // |obj| will be marked once |obj_node| gets processed.
    EXPECT_FALSE(obj->IsMarked());
  }
}

TEST(IncrementalMarkingTest, HeapDoublyLinkedListAppend) {
  Object* obj = Object::Create();
  ObjectNode* obj_node = new ObjectNode(obj);
  HeapDoublyLinkedList<ObjectNode> list;
  {
    ExpectWriteBarrierFires<ObjectNode> scope(ThreadState::Current(),
                                              {obj_node});
    list.Append(obj_node);
    // |obj| will be marked once |obj_node| gets processed.
    EXPECT_FALSE(obj->IsMarked());
  }
}

// =============================================================================
// HeapDeque support. ==========================================================
// =============================================================================

TEST(IncrementalMarkingTest, HeapDequePushBackMember) {
  Object* obj = Object::Create();
  HeapDeque<Member<Object>> deq;
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    deq.push_back(obj);
  }
}

TEST(IncrementalMarkingTest, HeapDequePushFrontMember) {
  Object* obj = Object::Create();
  HeapDeque<Member<Object>> deq;
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    deq.push_front(obj);
  }
}

TEST(IncrementalMarkingTest, HeapDequeEmplaceBackMember) {
  Object* obj = Object::Create();
  HeapDeque<Member<Object>> deq;
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    deq.emplace_back(obj);
  }
}

TEST(IncrementalMarkingTest, HeapDequeEmplaceFrontMember) {
  Object* obj = Object::Create();
  HeapDeque<Member<Object>> deq;
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    deq.emplace_front(obj);
  }
}

TEST(IncrementalMarkingTest, HeapDequeCopyMember) {
  Object* object = Object::Create();
  HeapDeque<Member<Object>> deq1;
  deq1.push_back(object);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {object});
    HeapDeque<Member<Object>> deq2(deq1);
  }
}

TEST(IncrementalMarkingTest, HeapDequeMoveMember) {
  Object* object = Object::Create();
  HeapDeque<Member<Object>> deq1;
  deq1.push_back(object);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {object});
    HeapDeque<Member<Object>> deq2(std::move(deq1));
  }
}

TEST(IncrementalMarkingTest, HeapDequeSwapMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapDeque<Member<Object>> deq1;
  deq1.push_back(obj1);
  HeapDeque<Member<Object>> deq2;
  deq2.push_back(obj2);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    std::swap(deq1, deq2);
  }
}

// =============================================================================
// HeapHashSet support. ========================================================
// =============================================================================

namespace {

template <typename Container>
void Insert() {
  Object* obj = Object::Create();
  Container container;
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    container.insert(obj);
  }
}

template <typename Container>
void InsertNoBarrier() {
  Object* obj = Object::Create();
  Container container;
  {
    ExpectNoWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    container.insert(obj);
  }
}

template <typename Container>
void Copy() {
  Object* obj = Object::Create();
  Container container1;
  container1.insert(obj);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    Container container2(container1);
    EXPECT_TRUE(container1.Contains(obj));
    EXPECT_TRUE(container2.Contains(obj));
  }
}

template <typename Container>
void CopyNoBarrier() {
  Object* obj = Object::Create();
  Container container1;
  container1.insert(obj);
  {
    ExpectNoWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    Container container2(container1);
    EXPECT_TRUE(container1.Contains(obj));
    EXPECT_TRUE(container2.Contains(obj));
  }
}

template <typename Container>
void Move() {
  Object* obj = Object::Create();
  Container container1;
  container1.insert(obj);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    Container container2(std::move(container1));
  }
}

template <typename Container>
void MoveNoBarrier() {
  Object* obj = Object::Create();
  Container container1;
  container1.insert(obj);
  {
    ExpectNoWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
    Container container2(std::move(container1));
  }
}

template <typename Container>
void Swap() {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  Container container1;
  container1.insert(obj1);
  Container container2;
  container2.insert(obj2);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    std::swap(container1, container2);
  }
}

template <typename Container>
void SwapNoBarrier() {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  Container container1;
  container1.insert(obj1);
  Container container2;
  container2.insert(obj2);
  {
    ExpectNoWriteBarrierFires<Object> scope(ThreadState::Current(),
                                            {obj1, obj2});
    std::swap(container1, container2);
  }
}

}  // namespace

TEST(IncrementalMarkingTest, HeapHashSetInsert) {
  Insert<HeapHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Insert<HeapHashSet<WeakMember<Object>>>();
}

TEST(IncrementalMarkingTest, HeapHashSetCopy) {
  Copy<HeapHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Copy<HeapHashSet<WeakMember<Object>>>();
}

TEST(IncrementalMarkingTest, HeapHashSetMove) {
  Move<HeapHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Move<HeapHashSet<WeakMember<Object>>>();
}

TEST(IncrementalMarkingTest, HeapHashSetSwap) {
  Swap<HeapHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Swap<HeapHashSet<WeakMember<Object>>>();
}

class StrongWeakPair : public std::pair<Member<Object>, WeakMember<Object>> {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

  typedef std::pair<Member<Object>, WeakMember<Object>> Base;

 public:
  StrongWeakPair(Object* obj1, Object* obj2) : Base(obj1, obj2) {}

  StrongWeakPair(WTF::HashTableDeletedValueType)
      : Base(WTF::kHashTableDeletedValue, nullptr) {}

  bool IsHashTableDeletedValue() const {
    return first.IsHashTableDeletedValue();
  }

  // Trace will be called for write barrier invocations. Only strong members
  // are interesting.
  void Trace(blink::Visitor* visitor) { visitor->Trace(first); }

  // TraceInCollection will be called for weak processing.
  template <typename VisitorDispatcher>
  bool TraceInCollection(VisitorDispatcher visitor,
                         WTF::WeakHandlingFlag weakness) {
    visitor->Trace(first);
    if (weakness == WTF::kNoWeakHandling) {
      visitor->Trace(second);
    }
    return false;
  }
};

}  // namespace incremental_marking_test
}  // namespace blink

namespace WTF {

template <>
struct HashTraits<blink::incremental_marking_test::StrongWeakPair>
    : SimpleClassHashTraits<blink::incremental_marking_test::StrongWeakPair> {
  static const WTF::WeakHandlingFlag kWeakHandlingFlag = WTF::kWeakHandling;

  template <typename U = void>
  struct IsTraceableInCollection {
    static const bool value = true;
  };

  static const bool kHasIsEmptyValueFunction = true;
  static bool IsEmptyValue(
      const blink::incremental_marking_test::StrongWeakPair& value) {
    return !value.first;
  }

  static void ConstructDeletedValue(
      blink::incremental_marking_test::StrongWeakPair& slot,
      bool) {
    new (NotNull, &slot)
        blink::incremental_marking_test::StrongWeakPair(kHashTableDeletedValue);
  }

  static bool IsDeletedValue(
      const blink::incremental_marking_test::StrongWeakPair& value) {
    return value.IsHashTableDeletedValue();
  }

  template <typename VisitorDispatcher>
  static bool TraceInCollection(
      VisitorDispatcher visitor,
      blink::incremental_marking_test::StrongWeakPair& t,
      WTF::WeakHandlingFlag weakness) {
    return t.TraceInCollection(visitor, weakness);
  }
};

template <>
struct DefaultHash<blink::incremental_marking_test::StrongWeakPair> {
  typedef PairHash<blink::Member<blink::incremental_marking_test::Object>,
                   blink::WeakMember<blink::incremental_marking_test::Object>>
      Hash;
};

template <>
struct IsTraceable<blink::incremental_marking_test::StrongWeakPair> {
  static const bool value = IsTraceable<std::pair<
      blink::Member<blink::incremental_marking_test::Object>,
      blink::WeakMember<blink::incremental_marking_test::Object>>>::value;
};

}  // namespace WTF

namespace blink {
namespace incremental_marking_test {

TEST(IncrementalMarkingTest, HeapHashSetStrongWeakPair) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashSet<StrongWeakPair> set;
  {
    // Only the strong field in the StrongWeakPair should be hit by the
    // write barrier.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1});
    set.insert(StrongWeakPair(obj1, obj2));
    EXPECT_FALSE(obj2->IsMarked());
  }
}

TEST(IncrementalMarkingTest, HeapLinkedHashSetStrongWeakPair) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapLinkedHashSet<StrongWeakPair> set;
  {
    // Only the strong field in the StrongWeakPair should be hit by the
    // write barrier.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1});
    set.insert(StrongWeakPair(obj1, obj2));
    EXPECT_FALSE(obj2->IsMarked());
  }
}

// =============================================================================
// HeapLinkedHashSet support. ==================================================
// =============================================================================

TEST(IncrementalMarkingTest, HeapLinkedHashSetInsert) {
  Insert<HeapLinkedHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Insert<HeapLinkedHashSet<WeakMember<Object>>>();
}

TEST(IncrementalMarkingTest, HeapLinkedHashSetCopy) {
  Copy<HeapLinkedHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Copy<HeapLinkedHashSet<WeakMember<Object>>>();
}

TEST(IncrementalMarkingTest, HeapLinkedHashSetMove) {
  Move<HeapLinkedHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Move<HeapLinkedHashSet<WeakMember<Object>>>();
}

TEST(IncrementalMarkingTest, HeapLinkedHashSetSwap) {
  Swap<HeapLinkedHashSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Move<HeapLinkedHashSet<WeakMember<Object>>>();
}

// =============================================================================
// HeapHashCountedSet support. =================================================
// =============================================================================

// HeapHashCountedSet does not support copy or move.

TEST(IncrementalMarkingTest, HeapHashCountedSetInsert) {
  Insert<HeapHashCountedSet<Member<Object>>>();
  // Weak references are strongified for the current cycle.
  Insert<HeapHashCountedSet<WeakMember<Object>>>();
}

TEST(IncrementalMarkingTest, HeapHashCountedSetSwap) {
  // HeapHashCountedSet is not move constructible so we cannot use std::swap.
  {
    Object* obj1 = Object::Create();
    Object* obj2 = Object::Create();
    HeapHashCountedSet<Member<Object>> container1;
    container1.insert(obj1);
    HeapHashCountedSet<Member<Object>> container2;
    container2.insert(obj2);
    {
      ExpectWriteBarrierFires<Object> scope(ThreadState::Current(),
                                            {obj1, obj2});
      container1.swap(container2);
    }
  }
  {
    Object* obj1 = Object::Create();
    Object* obj2 = Object::Create();
    HeapHashCountedSet<WeakMember<Object>> container1;
    container1.insert(obj1);
    HeapHashCountedSet<WeakMember<Object>> container2;
    container2.insert(obj2);
    {
      // Weak references are strongified for the current cycle.
      ExpectWriteBarrierFires<Object> scope(ThreadState::Current(),
                                            {obj1, obj2});
      container1.swap(container2);
    }
  }
}

// =============================================================================
// HeapTerminatedArray support. ================================================
// =============================================================================

class TerminatedArrayNode {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  TerminatedArrayNode(Object* obj) : obj_(obj), is_last_in_array_(false) {}

  // TerminatedArray support.
  bool IsLastInArray() const { return is_last_in_array_; }
  void SetLastInArray(bool flag) { is_last_in_array_ = flag; }

  void Trace(blink::Visitor* visitor) { visitor->Trace(obj_); }

 private:
  Member<Object> obj_;
  bool is_last_in_array_;
};

}  // namespace incremental_marking_test
}  // namespace blink

WTF_ALLOW_MOVE_INIT_AND_COMPARE_WITH_MEM_FUNCTIONS(
    blink::incremental_marking_test::TerminatedArrayNode);

namespace blink {
namespace incremental_marking_test {

TEST(IncrementalMarkingTest, HeapTerminatedArrayBuilder) {
  Object* obj = Object::Create();
  HeapTerminatedArray<TerminatedArrayNode>* array = nullptr;
  {
    // The builder allocates the backing store on Oilpans heap, effectively
    // triggering a write barrier.
    HeapTerminatedArrayBuilder<TerminatedArrayNode> builder(array);
    builder.Grow(1);
    {
      ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj});
      builder.Append(TerminatedArrayNode(obj));
    }
    array = builder.Release();
  }
}

// =============================================================================
// HeapHashMap support. ========================================================
// =============================================================================

TEST(IncrementalMarkingTest, HeapHashMapInsertMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<Member<Object>, Member<Object>> map;
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    map.insert(obj1, obj2);
  }
}

TEST(IncrementalMarkingTest, HeapHashMapInsertWeakMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<WeakMember<Object>, WeakMember<Object>> map;
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    map.insert(obj1, obj2);
  }
}

TEST(IncrementalMarkingTest, HeapHashMapInsertMemberWeakMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<Member<Object>, WeakMember<Object>> map;
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    map.insert(obj1, obj2);
  }
}

TEST(IncrementalMarkingTest, HeapHashMapInsertWeakMemberMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<WeakMember<Object>, Member<Object>> map;
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    map.insert(obj1, obj2);
  }
}

TEST(IncrementalMarkingTest, HeapHashMapSetMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<Member<Object>, Member<Object>> map;
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    map.Set(obj1, obj2);
  }
}

TEST(IncrementalMarkingTest, HeapHashMapSetMemberUpdateValue) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  Object* obj3 = Object::Create();
  HeapHashMap<Member<Object>, Member<Object>> map;
  map.insert(obj1, obj2);
  {
    // Only |obj3| is newly added to |map|, so we only expect the barrier to
    // fire on this one.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj3});
    map.Set(obj1, obj3);
    EXPECT_FALSE(HeapObjectHeader::FromPayload(obj1)->IsMarked());
    EXPECT_FALSE(HeapObjectHeader::FromPayload(obj2)->IsMarked());
  }
}

TEST(IncrementalMarkingTest, HeapHashMapIteratorChangeKey) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  Object* obj3 = Object::Create();
  HeapHashMap<Member<Object>, Member<Object>> map;
  map.insert(obj1, obj2);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj3});
    auto it = map.find(obj1);
    EXPECT_NE(map.end(), it);
    it->key = obj3;
  }
}

TEST(IncrementalMarkingTest, HeapHashMapIteratorChangeValue) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  Object* obj3 = Object::Create();
  HeapHashMap<Member<Object>, Member<Object>> map;
  map.insert(obj1, obj2);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj3});
    auto it = map.find(obj1);
    EXPECT_NE(map.end(), it);
    it->value = obj3;
  }
}

TEST(IncrementalMarkingTest, HeapHashMapCopyMemberMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<Member<Object>, Member<Object>> map1;
  map1.insert(obj1, obj2);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    EXPECT_TRUE(map1.Contains(obj1));
    HeapHashMap<Member<Object>, Member<Object>> map2(map1);
    EXPECT_TRUE(map1.Contains(obj1));
    EXPECT_TRUE(map2.Contains(obj1));
  }
}

TEST(IncrementalMarkingTest, HeapHashMapCopyWeakMemberWeakMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<WeakMember<Object>, WeakMember<Object>> map1;
  map1.insert(obj1, obj2);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    EXPECT_TRUE(map1.Contains(obj1));
    HeapHashMap<WeakMember<Object>, WeakMember<Object>> map2(map1);
    EXPECT_TRUE(map1.Contains(obj1));
    EXPECT_TRUE(map2.Contains(obj1));
  }
}

TEST(IncrementalMarkingTest, HeapHashMapCopyMemberWeakMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<Member<Object>, WeakMember<Object>> map1;
  map1.insert(obj1, obj2);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    EXPECT_TRUE(map1.Contains(obj1));
    HeapHashMap<Member<Object>, WeakMember<Object>> map2(map1);
    EXPECT_TRUE(map1.Contains(obj1));
    EXPECT_TRUE(map2.Contains(obj1));
  }
}

TEST(IncrementalMarkingTest, HeapHashMapCopyWeakMemberMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<WeakMember<Object>, Member<Object>> map1;
  map1.insert(obj1, obj2);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    EXPECT_TRUE(map1.Contains(obj1));
    HeapHashMap<WeakMember<Object>, Member<Object>> map2(map1);
    EXPECT_TRUE(map1.Contains(obj1));
    EXPECT_TRUE(map2.Contains(obj1));
  }
}

TEST(IncrementalMarkingTest, HeapHashMapMoveMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<Member<Object>, Member<Object>> map1;
  map1.insert(obj1, obj2);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    HeapHashMap<Member<Object>, Member<Object>> map2(std::move(map1));
  }
}

TEST(IncrementalMarkingTest, HeapHashMapMoveWeakMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<WeakMember<Object>, WeakMember<Object>> map1;
  map1.insert(obj1, obj2);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    HeapHashMap<WeakMember<Object>, WeakMember<Object>> map2(std::move(map1));
  }
}

TEST(IncrementalMarkingTest, HeapHashMapMoveMemberWeakMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<Member<Object>, WeakMember<Object>> map1;
  map1.insert(obj1, obj2);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    HeapHashMap<Member<Object>, WeakMember<Object>> map2(std::move(map1));
  }
}

TEST(IncrementalMarkingTest, HeapHashMapMoveWeakMemberMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<WeakMember<Object>, Member<Object>> map1;
  map1.insert(obj1, obj2);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    HeapHashMap<WeakMember<Object>, Member<Object>> map2(std::move(map1));
  }
}

TEST(IncrementalMarkingTest, HeapHashMapSwapMemberMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  Object* obj3 = Object::Create();
  Object* obj4 = Object::Create();
  HeapHashMap<Member<Object>, Member<Object>> map1;
  map1.insert(obj1, obj2);
  HeapHashMap<Member<Object>, Member<Object>> map2;
  map2.insert(obj3, obj4);
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(),
                                          {obj1, obj2, obj3, obj4});
    std::swap(map1, map2);
  }
}

TEST(IncrementalMarkingTest, HeapHashMapSwapWeakMemberWeakMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  Object* obj3 = Object::Create();
  Object* obj4 = Object::Create();
  HeapHashMap<WeakMember<Object>, WeakMember<Object>> map1;
  map1.insert(obj1, obj2);
  HeapHashMap<WeakMember<Object>, WeakMember<Object>> map2;
  map2.insert(obj3, obj4);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(),
                                          {obj1, obj2, obj3, obj4});
    std::swap(map1, map2);
  }
}

TEST(IncrementalMarkingTest, HeapHashMapSwapMemberWeakMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  Object* obj3 = Object::Create();
  Object* obj4 = Object::Create();
  HeapHashMap<Member<Object>, WeakMember<Object>> map1;
  map1.insert(obj1, obj2);
  HeapHashMap<Member<Object>, WeakMember<Object>> map2;
  map2.insert(obj3, obj4);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(),
                                          {obj1, obj2, obj3, obj4});
    std::swap(map1, map2);
  }
}

TEST(IncrementalMarkingTest, HeapHashMapSwapWeakMemberMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  Object* obj3 = Object::Create();
  Object* obj4 = Object::Create();
  HeapHashMap<WeakMember<Object>, Member<Object>> map1;
  map1.insert(obj1, obj2);
  HeapHashMap<WeakMember<Object>, Member<Object>> map2;
  map2.insert(obj3, obj4);
  {
    // Weak references are strongified for the current cycle.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(),
                                          {obj1, obj2, obj3, obj4});
    std::swap(map1, map2);
  }
}

TEST(IncrementalMarkingTest, HeapHashMapInsertStrongWeakPairMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  Object* obj3 = Object::Create();
  HeapHashMap<StrongWeakPair, Member<Object>> map;
  {
    // Tests that the write barrier also fires for entities such as
    // StrongWeakPair that don't overload assignment operators in translators.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj3});
    map.insert(StrongWeakPair(obj1, obj2), obj3);
  }
}

TEST(IncrementalMarkingTest, HeapHashMapInsertMemberStrongWeakPair) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  Object* obj3 = Object::Create();
  HeapHashMap<Member<Object>, StrongWeakPair> map;
  {
    // Tests that the write barrier also fires for entities such as
    // StrongWeakPair that don't overload assignment operators in translators.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1, obj2});
    map.insert(obj1, StrongWeakPair(obj2, obj3));
  }
}

TEST(IncrementalMarkingTest, HeapHashMapCopyKeysToVectorMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<Member<Object>, Member<Object>> map;
  map.insert(obj1, obj2);
  HeapVector<Member<Object>> vec;
  {
    // Only key should have its write barrier fired. A write barrier call for
    // value hints to an inefficient implementation.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj1});
    CopyKeysToVector(map, vec);
  }
}

TEST(IncrementalMarkingTest, HeapHashMapCopyValuesToVectorMember) {
  Object* obj1 = Object::Create();
  Object* obj2 = Object::Create();
  HeapHashMap<Member<Object>, Member<Object>> map;
  map.insert(obj1, obj2);
  HeapVector<Member<Object>> vec;
  {
    // Only value should have its write barrier fired. A write barrier call for
    // key hints to an inefficient implementation.
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {obj2});
    CopyValuesToVector(map, vec);
  }
}

// TODO(keishi) Non-weak hash table backings should be promptly freed but they
// are currently not because we emit write barriers for the backings, and we
// don't free marked backings.
TEST(IncrementalMarkingTest, DISABLED_WeakHashMapPromptlyFreeDisabled) {
  ThreadState* state = ThreadState::Current();
  state->SetGCState(ThreadState::kIncrementalMarkingStartScheduled);
  state->SetGCState(ThreadState::kIncrementalMarkingStepScheduled);
  Persistent<Object> obj1 = Object::Create();
  NormalPageArena* arena = static_cast<NormalPageArena*>(
      ThreadState::Current()->Heap().Arena(BlinkGC::kHashTableArenaIndex));
  CHECK(arena);
  {
    size_t before = arena->promptly_freed_size();
    // Create two maps so we don't promptly free at the allocation point.
    HeapHashMap<WeakMember<Object>, Member<Object>> weak_map1;
    HeapHashMap<WeakMember<Object>, Member<Object>> weak_map2;
    weak_map1.insert(obj1, obj1);
    weak_map2.insert(obj1, obj1);
    weak_map1.clear();
    size_t after = arena->promptly_freed_size();
    // Weak hash table backings should not be promptly freed.
    EXPECT_EQ(after, before);
  }
  {
    size_t before = arena->promptly_freed_size();
    // Create two maps so we don't promptly free at the allocation point.
    HeapHashMap<Member<Object>, Member<Object>> map1;
    HeapHashMap<Member<Object>, Member<Object>> map2;
    map1.insert(obj1, obj1);
    map2.insert(obj1, obj1);
    map1.clear();
    size_t after = arena->promptly_freed_size();
    // Non-weak hash table backings should be promptly freed.
    EXPECT_GT(after, before);
  }
  state->SetGCState(ThreadState::kIncrementalMarkingFinalizeScheduled);
  state->SetGCState(ThreadState::kNoGCScheduled);
}

namespace {

class RegisteringMixin;
using ObjectRegistry = HeapHashMap<void*, Member<RegisteringMixin>>;

class RegisteringMixin : public GarbageCollectedMixin {
 public:
  explicit RegisteringMixin(ObjectRegistry* registry) {
    HeapObjectHeader* header = GetHeapObjectHeader();
    const void* uninitialized_value = BlinkGC::kNotFullyConstructedObject;
    EXPECT_EQ(uninitialized_value, header);
    registry->insert(reinterpret_cast<void*>(this), this);
  }
};

class RegisteringObject : public GarbageCollected<RegisteringObject>,
                          public RegisteringMixin {
  USING_GARBAGE_COLLECTED_MIXIN(RegisteringObject);

 public:
  explicit RegisteringObject(ObjectRegistry* registry)
      : RegisteringMixin(registry) {}
};

}  // namespace

TEST(IncrementalMarkingTest, WriteBarrierDuringMixinConstruction) {
  IncrementalMarkingScope scope(ThreadState::Current());
  ObjectRegistry registry;
  RegisteringObject* object = new RegisteringObject(&registry);
  EXPECT_FALSE(scope.marking_worklist()->IsGlobalEmpty());
  MarkingItem item;
  EXPECT_TRUE(scope.marking_worklist()->Pop(WorklistTaskId::MainThread, &item));
  RegisteringObject* recorded_object =
      reinterpret_cast<RegisteringObject*>(item.object);
  // In this case, the Member write barrier will also add the object to the
  // regular marking stack. The not-fully-constructed object marking stack is
  // only needed when going through custom off-heap data structures that require
  // eager tracing.
  EXPECT_EQ(object, recorded_object);
  // In this example, there are two more objects on the callback stack: the
  // backing store for which a write barrier triggers after rehashing, and
  // a write barrier for the Member assignment (which might not always happen).
  scope.marking_worklist()->Pop(WorklistTaskId::MainThread, &item);
  scope.marking_worklist()->Pop(WorklistTaskId::MainThread, &item);
  // The mixin object should be on the not-fully-constructed object marking
  // stack.
  EXPECT_FALSE(scope.not_fully_constructed_worklist()->IsGlobalEmpty());
  NotFullyConstructedItem item2;
  EXPECT_TRUE(scope.not_fully_constructed_worklist()->Pop(
      WorklistTaskId::MainThread, &item2));
  RegisteringObject* recorded_object2 =
      reinterpret_cast<RegisteringObject*>(item2);
  EXPECT_EQ(object, recorded_object2);
}

TEST(IncrementalMarkingTest, OverrideAfterMixinConstruction) {
  ObjectRegistry registry;
  RegisteringMixin* mixin = new RegisteringObject(&registry);
  HeapObjectHeader* header = mixin->GetHeapObjectHeader();
  const void* uninitialized_value = BlinkGC::kNotFullyConstructedObject;
  EXPECT_NE(uninitialized_value, header);
}

// =============================================================================
// Tests that execute complete incremental garbage collections. ================
// =============================================================================

// Test driver for incremental marking. Assumes that no stack handling is
// required.
class IncrementalMarkingTestDriver {
 public:
  explicit IncrementalMarkingTestDriver(ThreadState* thread_state)
      : thread_state_(thread_state) {}
  ~IncrementalMarkingTestDriver() {
    if (thread_state_->IsIncrementalMarking())
      FinishGC();
  }

  void Start() {
    thread_state_->ScheduleIncrementalMarkingStart();
    thread_state_->RunScheduledGC(BlinkGC::kNoHeapPointersOnStack);
  }

  bool SingleStep() {
    CHECK(thread_state_->IsIncrementalMarking());
    if (thread_state_->GcState() ==
        ThreadState::kIncrementalMarkingStepScheduled) {
      thread_state_->RunScheduledGC(BlinkGC::kNoHeapPointersOnStack);
      return true;
    }
    return false;
  }

  void FinishSteps() {
    CHECK(thread_state_->IsIncrementalMarking());
    while (SingleStep()) {
    }
  }

  void FinishGC() {
    CHECK(thread_state_->IsIncrementalMarking());
    FinishSteps();
    CHECK_EQ(ThreadState::kIncrementalMarkingFinalizeScheduled,
             thread_state_->GcState());
    thread_state_->RunScheduledGC(BlinkGC::kNoHeapPointersOnStack);
    CHECK(!thread_state_->IsIncrementalMarking());
    thread_state_->CompleteSweep();
  }

 private:
  ThreadState* const thread_state_;
};

TEST(IncrementalMarkingTest, TestDriver) {
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.Start();
  EXPECT_TRUE(ThreadState::Current()->IsIncrementalMarking());
  driver.SingleStep();
  EXPECT_TRUE(ThreadState::Current()->IsIncrementalMarking());
  driver.FinishGC();
  EXPECT_FALSE(ThreadState::Current()->IsIncrementalMarking());
}

TEST(IncrementalMarkingTest, DropBackingStore) {
  // Regression test: crbug.com/828537
  using WeakStore = HeapHashCountedSet<WeakMember<Object>>;

  Persistent<WeakStore> persistent(new WeakStore);
  persistent->insert(Object::Create());
  IncrementalMarkingTestDriver driver(ThreadState::Current());
  driver.Start();
  driver.FinishSteps();
  persistent->clear();
  // Marking verifier should not crash on a black backing store with all
  // black->white edges.
  driver.FinishGC();
}

}  // namespace incremental_marking_test
}  // namespace blink

#endif  // BUILDFLAG(BLINK_HEAP_INCREMENTAL_MARKING)
