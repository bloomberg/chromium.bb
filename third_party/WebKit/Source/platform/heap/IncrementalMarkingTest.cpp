// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/heap/CallbackStack.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Heap.h"
#include "platform/heap/Member.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/TraceTraits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace incremental_marking_test {

class Dummy : public GarbageCollected<Dummy> {
 public:
  static Dummy* Create() { return new Dummy(); }

  void set_next(Dummy* next) { next_ = next; }

  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(next_); }

 private:
  Dummy() : next_(nullptr) {}

  Member<Dummy> next_;
};

TEST(IncrementalMarkingTest, EnableDisableBarrier) {
  Dummy* dummy = Dummy::Create();
  BasePage* page = PageFromObject(dummy);
  ThreadHeap& heap = ThreadState::Current()->Heap();
  EXPECT_FALSE(page->IsIncrementalMarking());
  heap.EnableIncrementalMarkingBarrier();
  EXPECT_TRUE(page->IsIncrementalMarking());
  heap.DisableIncrementalMarkingBarrier();
  EXPECT_FALSE(page->IsIncrementalMarking());
}

TEST(IncrementalMarkingTest, WriteBarrierOnUnmarkedObject) {
  Dummy* parent = Dummy::Create();
  Dummy* child = Dummy::Create();
  HeapObjectHeader* child_header = HeapObjectHeader::FromPayload(child);

  ThreadHeap& heap = ThreadState::Current()->Heap();
  CallbackStack* marking_stack = heap.MarkingStack();

  EXPECT_TRUE(marking_stack->IsEmpty());
  marking_stack->Commit();
  heap.EnableIncrementalMarkingBarrier();
  EXPECT_FALSE(child_header->IsMarked());
  parent->set_next(child);
  EXPECT_TRUE(child_header->IsMarked());
  EXPECT_FALSE(marking_stack->IsEmpty());
  CallbackStack::Item* item = marking_stack->Pop();
  EXPECT_EQ(child, item->Object());
  heap.DisableIncrementalMarkingBarrier();
  marking_stack->Decommit();
}

TEST(IncrementalMarkingTest, NoWriteBarrierOnMarkedObject) {
  Dummy* parent = Dummy::Create();
  Dummy* child = Dummy::Create();
  HeapObjectHeader* child_header = HeapObjectHeader::FromPayload(child);
  child_header->Mark();

  ThreadHeap& heap = ThreadState::Current()->Heap();
  CallbackStack* marking_stack = heap.MarkingStack();

  EXPECT_TRUE(marking_stack->IsEmpty());
  heap.EnableIncrementalMarkingBarrier();
  EXPECT_TRUE(child_header->IsMarked());
  parent->set_next(child);
  EXPECT_TRUE(marking_stack->IsEmpty());
  heap.DisableIncrementalMarkingBarrier();
}

namespace {

class Mixin : public GarbageCollectedMixin {
 public:
  Mixin() : next_(nullptr) {}
  virtual ~Mixin() {}

  virtual void Trace(blink::Visitor* visitor) { visitor->Trace(next_); }

  virtual void Bar() {}

 protected:
  Member<Dummy> next_;
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
  HeapObjectHeader* child_header = HeapObjectHeader::FromPayload(child);

  ThreadHeap& heap = ThreadState::Current()->Heap();
  CallbackStack* marking_stack = heap.MarkingStack();

  Mixin* mixin = static_cast<Mixin*>(child);
  EXPECT_NE(static_cast<void*>(child), static_cast<void*>(mixin));

  EXPECT_TRUE(marking_stack->IsEmpty());
  marking_stack->Commit();
  heap.EnableIncrementalMarkingBarrier();
  EXPECT_FALSE(child_header->IsMarked());
  parent->set_mixin(mixin);
  EXPECT_TRUE(child_header->IsMarked());
  EXPECT_FALSE(marking_stack->IsEmpty());
  CallbackStack::Item* item = marking_stack->Pop();
  EXPECT_EQ(child, item->Object());
  heap.DisableIncrementalMarkingBarrier();
  marking_stack->Decommit();
}

TEST(IncrementalMarkingTest, NoWriteBarrierOnMarkedMixinApplication) {
  ParentWithMixinPointer* parent = ParentWithMixinPointer::Create();
  Child* child = Child::Create();
  HeapObjectHeader* child_header = HeapObjectHeader::FromPayload(child);
  child_header->Mark();

  ThreadHeap& heap = ThreadState::Current()->Heap();
  CallbackStack* marking_stack = heap.MarkingStack();

  Mixin* mixin = static_cast<Mixin*>(child);
  EXPECT_NE(static_cast<void*>(child), static_cast<void*>(mixin));

  EXPECT_TRUE(marking_stack->IsEmpty());
  heap.EnableIncrementalMarkingBarrier();
  EXPECT_TRUE(child_header->IsMarked());
  parent->set_mixin(mixin);
  EXPECT_TRUE(marking_stack->IsEmpty());
  heap.DisableIncrementalMarkingBarrier();
}

}  // namespace incremental_marking_test
}  // namespace blink
