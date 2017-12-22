// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>
#include <vector>

#include "platform/heap/CallbackStack.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Heap.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/heap/IncrementalMarkingFlag.h"
#include "platform/heap/Member.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/TraceTraits.h"
#include "platform/heap/Visitor.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(BLINK_HEAP_INCREMENTAL_MARKING)

namespace blink {
namespace incremental_marking_test {

class IncrementalMarkingScope {
 public:
  explicit IncrementalMarkingScope(ThreadState* thread_state)
      : gc_forbidden_scope_(thread_state),
        thread_state_(thread_state),
        heap_(thread_state_->Heap()),
        marking_stack_(heap_.MarkingStack()) {
    EXPECT_TRUE(marking_stack_->IsEmpty());
    marking_stack_->Commit();
    heap_.EnableIncrementalMarkingBarrier();
    thread_state->current_gc_data_.visitor =
        Visitor::Create(thread_state, Visitor::kGlobalMarking);
  }

  ~IncrementalMarkingScope() {
    EXPECT_TRUE(marking_stack_->IsEmpty());
    heap_.DisableIncrementalMarkingBarrier();
    marking_stack_->Decommit();
  }

  CallbackStack* marking_stack() const { return marking_stack_; }
  ThreadHeap& heap() const { return heap_; }

 protected:
  ThreadState::GCForbiddenScope gc_forbidden_scope_;
  ThreadState* const thread_state_;
  ThreadHeap& heap_;
  CallbackStack* const marking_stack_;
};

template <typename T>
class ExpectWriteBarrierFires : public IncrementalMarkingScope {
 public:
  ExpectWriteBarrierFires(ThreadState* thread_state,
                          std::initializer_list<T*> objects)
      : IncrementalMarkingScope(thread_state), objects_(objects) {
    EXPECT_TRUE(marking_stack_->IsEmpty());
    for (T* object : objects_) {
      headers_.push_back(HeapObjectHeader::FromPayload(object));
      EXPECT_FALSE(headers_.back()->IsMarked());
    }
    EXPECT_FALSE(objects_.empty());
  }

  ~ExpectWriteBarrierFires() {
    EXPECT_FALSE(marking_stack_->IsEmpty());
    // All headers of objects watched should be marked.
    for (HeapObjectHeader* header : headers_) {
      EXPECT_TRUE(header->IsMarked());
    }
    // All objects watched should be on the marking stack.
    while (!marking_stack_->IsEmpty()) {
      CallbackStack::Item* item = marking_stack_->Pop();
      T* obj = reinterpret_cast<T*>(item->Object());
      auto pos = std::find(objects_.begin(), objects_.end(), obj);
      EXPECT_NE(objects_.end(), pos);
      objects_.erase(pos);
    }
    EXPECT_TRUE(objects_.empty());
  }

 private:
  std::vector<T*> objects_;
  std::vector<HeapObjectHeader*> headers_;
};

template <typename T>
class ExpectNoWriteBarrierFires : public IncrementalMarkingScope {
 public:
  ExpectNoWriteBarrierFires(ThreadState* thread_state, T* object)
      : IncrementalMarkingScope(thread_state),
        object_(object),
        header_(HeapObjectHeader::FromPayload(object_)) {
    EXPECT_TRUE(marking_stack_->IsEmpty());
    EXPECT_TRUE(header_->IsMarked());
  }

  ~ExpectNoWriteBarrierFires() {
    EXPECT_TRUE(marking_stack_->IsEmpty());
    EXPECT_TRUE(header_->IsMarked());
  }

 private:
  T* const object_;
  HeapObjectHeader* const header_;
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
  Object* object = Object::Create();
  BasePage* page = PageFromObject(object);
  ThreadHeap& heap = ThreadState::Current()->Heap();
  EXPECT_FALSE(page->IsIncrementalMarking());
  EXPECT_FALSE(ThreadState::Current()->IsIncrementalMarking());
  heap.EnableIncrementalMarkingBarrier();
  EXPECT_TRUE(page->IsIncrementalMarking());
  EXPECT_TRUE(ThreadState::Current()->IsIncrementalMarking());
  heap.DisableIncrementalMarkingBarrier();
  EXPECT_FALSE(page->IsIncrementalMarking());
  EXPECT_FALSE(ThreadState::Current()->IsIncrementalMarking());
}

TEST(IncrementalMarkingTest, StackFrameDepthDisabled) {
  IncrementalMarkingScope scope(ThreadState::Current());
  EXPECT_FALSE(scope.heap().GetStackFrameDepth().IsSafeToRecurse());
}

// =============================================================================
// Member<T> support. ==========================================================
// =============================================================================

TEST(IncrementalMarkingTest, WriteBarrierOnUnmarkedObject) {
  Object* parent = Object::Create();
  Object* child = Object::Create();
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {child});
    EXPECT_FALSE(child->IsMarked());
    parent->set_next(child);
    EXPECT_TRUE(child->IsMarked());
  }
}

TEST(IncrementalMarkingTest, NoWriteBarrierOnMarkedObject) {
  Object* parent = Object::Create();
  Object* child = Object::Create();
  HeapObjectHeader::FromPayload(child)->Mark();
  {
    ExpectNoWriteBarrierFires<Object> scope(ThreadState::Current(), child);
    parent->set_next(child);
  }
}

TEST(IncrementalMarkingTest, ManualWriteBarrierTriggersWhenMarkingIsOn) {
  Object* object = Object::Create();
  {
    ExpectWriteBarrierFires<Object> scope(ThreadState::Current(), {object});
    EXPECT_FALSE(object->IsMarked());
    ThreadState::Current()->Heap().WriteBarrier(object);
    EXPECT_TRUE(object->IsMarked());
  }
}

TEST(IncrementalMarkingTest, ManualWriteBarrierBailoutWhenMarkingIsOff) {
  Object* object = Object::Create();
  ThreadHeap& heap = ThreadState::Current()->Heap();
  CallbackStack* marking_stack = heap.MarkingStack();
  EXPECT_TRUE(marking_stack->IsEmpty());
  EXPECT_FALSE(object->IsMarked());
  heap.WriteBarrier(object);
  EXPECT_FALSE(object->IsMarked());
  EXPECT_TRUE(marking_stack->IsEmpty());
}

TEST(IncrementalMarkingTest, InitializingStoreTriggersNoWriteBarrier) {
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
    ExpectNoWriteBarrierFires<Child> scope(ThreadState::Current(), child);
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

TEST(IncrementalMarkingTest, HeapVectorAssumptions) {
  static_assert(std::is_trivially_move_assignable<Member<Object>>::value,
                "Member<T> should not be trivially move assignable");
  static_assert(std::is_trivially_copy_assignable<Member<Object>>::value,
                "Member<T> should not be trivially copy assignable");
}

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

}  // namespace incremental_marking_test
}  // namespace blink

#endif  // BUILDFLAG(BLINK_HEAP_INCREMENTAL_MARKING)
