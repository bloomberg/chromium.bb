// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/bindings/ScriptWrappableVisitor.h"

#include "bindings/core/v8/ToV8ForCore.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8GCController.h"
#include "core/testing/DeathAwareScriptWrappable.h"
#include "platform/bindings/TraceWrapperV8Reference.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

static void PreciselyCollectGarbage() {
  ThreadState::Current()->CollectAllGarbage();
}

static void RunV8Scavenger(v8::Isolate* isolate) {
  V8GCController::CollectGarbage(isolate, true);
}

static void RunV8FullGc(v8::Isolate* isolate) {
  V8GCController::CollectGarbage(isolate, false);
}

static bool DequeContains(const WTF::Deque<WrapperMarkingData>& deque,
                          void* needle) {
  for (auto item : deque) {
    if (item.RawObjectPointer() == needle)
      return true;
  }
  return false;
}

TEST(ScriptWrappableVisitorTest, ScriptWrappableVisitorTracesWrappers) {
  V8TestingScope scope;
  ScriptWrappableVisitor* visitor =
      V8PerIsolateData::From(scope.GetIsolate())->GetScriptWrappableVisitor();
  DeathAwareScriptWrappable* target = DeathAwareScriptWrappable::Create();
  DeathAwareScriptWrappable* dependency = DeathAwareScriptWrappable::Create();
  target->SetRawDependency(dependency);

  // The graph needs to be set up before starting tracing as otherwise the
  // conservative write barrier would trigger.
  visitor->TracePrologue();

  HeapObjectHeader* target_header = HeapObjectHeader::FromPayload(target);
  HeapObjectHeader* dependency_header =
      HeapObjectHeader::FromPayload(dependency);

  EXPECT_TRUE(visitor->MarkingDeque()->IsEmpty());
  EXPECT_FALSE(target_header->IsWrapperHeaderMarked());
  EXPECT_FALSE(dependency_header->IsWrapperHeaderMarked());

  std::pair<void*, void*> pair = std::make_pair(
      const_cast<WrapperTypeInfo*>(target->GetWrapperTypeInfo()), target);
  visitor->RegisterV8Reference(pair);
  EXPECT_EQ(visitor->MarkingDeque()->size(), 1ul);

  visitor->AdvanceTracing(
      0, v8::EmbedderHeapTracer::AdvanceTracingActions(
             v8::EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION));
  EXPECT_EQ(visitor->MarkingDeque()->size(), 0ul);
  EXPECT_TRUE(target_header->IsWrapperHeaderMarked());
  EXPECT_TRUE(dependency_header->IsWrapperHeaderMarked());

  visitor->AbortTracing();
}

TEST(ScriptWrappableVisitorTest, OilpanCollectObjectsNotReachableFromV8) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  {
    v8::HandleScope handle_scope(isolate);
    DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::Create();
    DeathAwareScriptWrappable::ObserveDeathsOf(object);

    // Creates new V8 wrapper and associates it with global scope
    ToV8(object, scope.GetContext()->Global(), isolate);
  }

  RunV8Scavenger(isolate);
  RunV8FullGc(isolate);
  PreciselyCollectGarbage();

  EXPECT_TRUE(DeathAwareScriptWrappable::HasDied());
}

TEST(ScriptWrappableVisitorTest, OilpanDoesntCollectObjectsReachableFromV8) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::Create();
  DeathAwareScriptWrappable::ObserveDeathsOf(object);

  // Creates new V8 wrapper and associates it with global scope
  ToV8(object, scope.GetContext()->Global(), isolate);

  RunV8Scavenger(isolate);
  RunV8FullGc(isolate);
  PreciselyCollectGarbage();

  EXPECT_FALSE(DeathAwareScriptWrappable::HasDied());
}

TEST(ScriptWrappableVisitorTest, V8ReportsLiveObjectsDuringScavenger) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::Create();
  DeathAwareScriptWrappable::ObserveDeathsOf(object);

  v8::Local<v8::Value> wrapper =
      ToV8(object, scope.GetContext()->Global(), isolate);
  EXPECT_TRUE(wrapper->IsObject());
  v8::Local<v8::Object> wrapper_object = wrapper->ToObject();
  // V8 collects wrappers with unmodified maps (as they can be recreated
  // without loosing any data if needed). We need to create some property on
  // wrapper so V8 will not see it as unmodified.
  EXPECT_TRUE(wrapper_object->CreateDataProperty(scope.GetContext(), 1, wrapper)
                  .IsJust());

  RunV8Scavenger(isolate);
  PreciselyCollectGarbage();

  EXPECT_FALSE(DeathAwareScriptWrappable::HasDied());
}

TEST(ScriptWrappableVisitorTest, V8ReportsLiveObjectsDuringFullGc) {
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::Create();
  DeathAwareScriptWrappable::ObserveDeathsOf(object);

  ToV8(object, scope.GetContext()->Global(), isolate);

  RunV8Scavenger(isolate);
  RunV8FullGc(isolate);
  PreciselyCollectGarbage();

  EXPECT_FALSE(DeathAwareScriptWrappable::HasDied());
}

TEST(ScriptWrappableVisitorTest, OilpanClearsHeadersWhenObjectDied) {
  V8TestingScope scope;

  DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::Create();
  ScriptWrappableVisitor* visitor =
      V8PerIsolateData::From(scope.GetIsolate())->GetScriptWrappableVisitor();
  visitor->TracePrologue();
  auto header = HeapObjectHeader::FromPayload(object);
  visitor->HeadersToUnmark()->push_back(header);

  PreciselyCollectGarbage();

  EXPECT_FALSE(visitor->HeadersToUnmark()->Contains(header));
  visitor->AbortTracing();
}

TEST(ScriptWrappableVisitorTest, OilpanClearsMarkingDequeWhenObjectDied) {
  V8TestingScope scope;

  DeathAwareScriptWrappable* object = DeathAwareScriptWrappable::Create();
  ScriptWrappableVisitor* visitor =
      V8PerIsolateData::From(scope.GetIsolate())->GetScriptWrappableVisitor();
  visitor->TracePrologue();

  visitor->MarkAndPushToMarkingDeque(object);

  EXPECT_EQ(visitor->MarkingDeque()->front().RawObjectPointer(), object);

  PreciselyCollectGarbage();

  EXPECT_EQ(visitor->MarkingDeque()->front().RawObjectPointer(), nullptr);

  visitor->AbortTracing();
}

TEST(ScriptWrappableVisitorTest,
     MarkedObjectDoesNothingOnWriteBarrierHitWhenDependencyIsMarkedToo) {
  V8TestingScope scope;

  ScriptWrappableVisitor* visitor =
      V8PerIsolateData::From(scope.GetIsolate())->GetScriptWrappableVisitor();
  visitor->TracePrologue();

  DeathAwareScriptWrappable* target = DeathAwareScriptWrappable::Create();
  DeathAwareScriptWrappable* dependencies[] = {
      DeathAwareScriptWrappable::Create(), DeathAwareScriptWrappable::Create(),
      DeathAwareScriptWrappable::Create(), DeathAwareScriptWrappable::Create(),
      DeathAwareScriptWrappable::Create()};

  HeapObjectHeader::FromPayload(target)->MarkWrapperHeader();
  for (int i = 0; i < 5; i++) {
    HeapObjectHeader::FromPayload(dependencies[i])->MarkWrapperHeader();
  }

  EXPECT_TRUE(visitor->MarkingDeque()->IsEmpty());

  target->SetRawDependency(dependencies[0]);
  target->SetWrappedDependency(dependencies[1]);
  target->AddWrappedVectorDependency(dependencies[2]);
  target->AddWrappedHashMapDependency(dependencies[3], dependencies[4]);

  EXPECT_TRUE(visitor->MarkingDeque()->IsEmpty());
  visitor->AbortTracing();
}

TEST(ScriptWrappableVisitorTest,
     MarkedObjectMarksDependencyOnWriteBarrierHitWhenNotMarked) {
  V8TestingScope scope;

  ScriptWrappableVisitor* visitor =
      V8PerIsolateData::From(scope.GetIsolate())->GetScriptWrappableVisitor();
  visitor->TracePrologue();

  DeathAwareScriptWrappable* target = DeathAwareScriptWrappable::Create();
  DeathAwareScriptWrappable* dependencies[] = {
      DeathAwareScriptWrappable::Create(), DeathAwareScriptWrappable::Create(),
      DeathAwareScriptWrappable::Create(), DeathAwareScriptWrappable::Create(),
      DeathAwareScriptWrappable::Create()};

  HeapObjectHeader::FromPayload(target)->MarkWrapperHeader();

  EXPECT_TRUE(visitor->MarkingDeque()->IsEmpty());

  target->SetRawDependency(dependencies[0]);
  target->SetWrappedDependency(dependencies[1]);
  target->AddWrappedVectorDependency(dependencies[2]);
  target->AddWrappedHashMapDependency(dependencies[3], dependencies[4]);

  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(DequeContains(*visitor->MarkingDeque(), dependencies[i]));
  }

  visitor->AbortTracing();
}

namespace {

class HandleContainer
    : public blink::GarbageCollectedFinalized<HandleContainer>,
      blink::TraceWrapperBase {
 public:
  static HandleContainer* Create() { return new HandleContainer(); }
  virtual ~HandleContainer() {}

  DEFINE_INLINE_TRACE() {}
  DEFINE_INLINE_TRACE_WRAPPERS() {
    visitor->TraceWrappers(handle_.Cast<v8::Value>());
  }

  void SetValue(v8::Isolate* isolate, v8::Local<v8::String> string) {
    handle_.Set(isolate, string);
  }

 private:
  HandleContainer() : handle_(this) {}

  TraceWrapperV8Reference<v8::String> handle_;
};

class InterceptingScriptWrappableVisitor
    : public blink::ScriptWrappableVisitor {
 public:
  InterceptingScriptWrappableVisitor(v8::Isolate* isolate)
      : ScriptWrappableVisitor(isolate), marked_wrappers_(new size_t(0)) {}
  ~InterceptingScriptWrappableVisitor() { delete marked_wrappers_; }

  virtual void MarkWrapper(const v8::PersistentBase<v8::Value>* handle) const {
    *marked_wrappers_ += 1;
    // Do not actually mark this visitor, as this would call into v8, which
    // would require executing an actual GC.
  }

  size_t NumberOfMarkedWrappers() const { return *marked_wrappers_; }

  void Start() { TracePrologue(); }

  void end() {
    // Gracefully terminate tracing.
    AdvanceTracing(
        0,
        v8::EmbedderHeapTracer::AdvanceTracingActions(
            v8::EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION));
    AbortTracing();
  }

 private:
  size_t* marked_wrappers_;  // Indirection required because of const override.
};

class InterceptingScriptWrappableVisitorScope
    : public V8PerIsolateData::TemporaryScriptWrappableVisitorScope {
  WTF_MAKE_NONCOPYABLE(InterceptingScriptWrappableVisitorScope);
  STACK_ALLOCATED();

 public:
  InterceptingScriptWrappableVisitorScope(v8::Isolate* isolate)
      : V8PerIsolateData::TemporaryScriptWrappableVisitorScope(
            isolate,
            std::unique_ptr<InterceptingScriptWrappableVisitor>(
                new InterceptingScriptWrappableVisitor(isolate))) {
    Visitor()->Start();
  }

  virtual ~InterceptingScriptWrappableVisitorScope() { Visitor()->end(); }

  InterceptingScriptWrappableVisitor* Visitor() {
    return reinterpret_cast<InterceptingScriptWrappableVisitor*>(
        CurrentVisitor());
  }
};

}  // namespace

TEST(ScriptWrappableVisitorTest, WriteBarrierOnUnmarkedContainer) {
  V8TestingScope scope;
  InterceptingScriptWrappableVisitorScope visitor_scope(scope.GetIsolate());
  auto* raw_visitor = visitor_scope.Visitor();

  v8::Local<v8::String> str =
      v8::String::NewFromUtf8(scope.GetIsolate(), "teststring",
                              v8::NewStringType::kNormal, sizeof("teststring"))
          .ToLocalChecked();
  HandleContainer* container = HandleContainer::Create();
  CHECK_EQ(0u, raw_visitor->NumberOfMarkedWrappers());
  container->SetValue(scope.GetIsolate(), str);
  // The write barrier is conservative and does not check the mark bits of the
  // source container.
  CHECK_EQ(1u, raw_visitor->NumberOfMarkedWrappers());
}

TEST(ScriptWrappableVisitorTest, WriteBarrierTriggersOnMarkedContainer) {
  V8TestingScope scope;
  InterceptingScriptWrappableVisitorScope visitor_scope(scope.GetIsolate());
  auto* raw_visitor = visitor_scope.Visitor();

  v8::Local<v8::String> str =
      v8::String::NewFromUtf8(scope.GetIsolate(), "teststring",
                              v8::NewStringType::kNormal, sizeof("teststring"))
          .ToLocalChecked();
  HandleContainer* container = HandleContainer::Create();
  HeapObjectHeader::FromPayload(container)->MarkWrapperHeader();
  CHECK_EQ(0u, raw_visitor->NumberOfMarkedWrappers());
  container->SetValue(scope.GetIsolate(), str);
  CHECK_EQ(1u, raw_visitor->NumberOfMarkedWrappers());
}

TEST(ScriptWrappableVisitorTest, VtableAtObjectStart) {
  // This test makes sure that the subobject v8::EmbedderHeapTracer is placed
  // at the start of a ScriptWrappableVisitor object. We do this to mitigate
  // potential problems that could be caused by LTO when passing
  // v8::EmbedderHeapTracer across the API boundary.
  V8TestingScope scope;
  std::unique_ptr<blink::ScriptWrappableVisitor> visitor(
      new ScriptWrappableVisitor(scope.GetIsolate()));
  CHECK_EQ(
      static_cast<void*>(visitor.get()),
      static_cast<void*>(dynamic_cast<v8::EmbedderHeapTracer*>(visitor.get())));
}

TEST(ScriptWrappableVisitor, WriteBarrierForScriptWrappable) {
  // Regression test for crbug.com/702490.
  V8TestingScope scope;
  InterceptingScriptWrappableVisitorScope visitor_scope(scope.GetIsolate());
  auto* raw_visitor = visitor_scope.Visitor();

  // Mark the ScriptWrappable.
  DeathAwareScriptWrappable* target = DeathAwareScriptWrappable::Create();
  HeapObjectHeader::FromPayload(target)->MarkWrapperHeader();

  // Create a 'wrapper' object.
  v8::Local<v8::ObjectTemplate> t = v8::ObjectTemplate::New(scope.GetIsolate());
  t->SetInternalFieldCount(2);
  v8::Local<v8::Object> obj =
      t->NewInstance(scope.GetContext()).ToLocalChecked();

  // Upon setting the wrapper we should have executed the write barrier.
  CHECK_EQ(0u, raw_visitor->NumberOfMarkedWrappers());
  bool success =
      target->SetWrapper(scope.GetIsolate(), target->GetWrapperTypeInfo(), obj);
  CHECK(success);
  CHECK_EQ(1u, raw_visitor->NumberOfMarkedWrappers());
}

TEST(ScriptWrappableVisitorTest, WriteBarrierOnHeapVectorSwap1) {
  V8TestingScope scope;
  ScriptWrappableVisitor* visitor =
      V8PerIsolateData::From(scope.GetIsolate())->GetScriptWrappableVisitor();

  HeapVector<DeathAwareScriptWrappable::Wrapper> vector1;
  DeathAwareScriptWrappable* entry1 = DeathAwareScriptWrappable::Create();
  vector1.push_back(entry1);
  HeapVector<DeathAwareScriptWrappable::Wrapper> vector2;
  DeathAwareScriptWrappable* entry2 = DeathAwareScriptWrappable::Create();
  vector2.push_back(entry2);

  visitor->TracePrologue();

  EXPECT_TRUE(visitor->MarkingDeque()->IsEmpty());
  swap(vector1, vector2);

  EXPECT_TRUE(DequeContains(*visitor->MarkingDeque(), entry1));
  EXPECT_TRUE(DequeContains(*visitor->MarkingDeque(), entry2));

  visitor->AbortTracing();
}

TEST(ScriptWrappableVisitorTest, WriteBarrierOnHeapVectorSwap2) {
  V8TestingScope scope;
  ScriptWrappableVisitor* visitor =
      V8PerIsolateData::From(scope.GetIsolate())->GetScriptWrappableVisitor();

  HeapVector<DeathAwareScriptWrappable::Wrapper> vector1;
  DeathAwareScriptWrappable* entry1 = DeathAwareScriptWrappable::Create();
  vector1.push_back(entry1);
  HeapVector<Member<DeathAwareScriptWrappable>> vector2;
  DeathAwareScriptWrappable* entry2 = DeathAwareScriptWrappable::Create();
  vector2.push_back(entry2);

  visitor->TracePrologue();

  EXPECT_TRUE(visitor->MarkingDeque()->IsEmpty());
  swap(vector1, vector2);

  // Only entry2 is held alive by TraceWrapperMember, so we only expect this
  // barrier to fire.
  EXPECT_TRUE(DequeContains(*visitor->MarkingDeque(), entry2));

  visitor->AbortTracing();
}

namespace {

class Mixin : public GarbageCollectedMixin {
 public:
  explicit Mixin(DeathAwareScriptWrappable* wrapper_in_mixin)
      : wrapper_in_mixin_(wrapper_in_mixin) {}

  DEFINE_INLINE_TRACE_WRAPPERS() { visitor->TraceWrappers(wrapper_in_mixin_); }

 protected:
  DeathAwareScriptWrappable::Wrapper wrapper_in_mixin_;
};

class ClassWithField {
 protected:
  int field_;
};

class Base : public blink::GarbageCollected<Base>,
             public blink::TraceWrapperBase,
             public ClassWithField,
             public Mixin {
  USING_GARBAGE_COLLECTED_MIXIN(Base);

 public:
  static Base* Create(DeathAwareScriptWrappable* wrapper_in_base,
                      DeathAwareScriptWrappable* wrapper_in_mixin) {
    return new Base(wrapper_in_base, wrapper_in_mixin);
  }

  DEFINE_INLINE_VIRTUAL_TRACE_WRAPPERS() {
    visitor->TraceWrappers(wrapper_in_base_);
    Mixin::TraceWrappers(visitor);
  }

 protected:
  Base(DeathAwareScriptWrappable* wrapper_in_base,
       DeathAwareScriptWrappable* wrapper_in_mixin)
      : Mixin(wrapper_in_mixin), wrapper_in_base_(wrapper_in_base) {
    // Use field_;
    field_ = 0;
  }

  DeathAwareScriptWrappable::Wrapper wrapper_in_base_;
};

}  // namespace

TEST(ScriptWrappableVisitorTest, MixinTracing) {
  V8TestingScope scope;
  ScriptWrappableVisitor* visitor =
      V8PerIsolateData::From(scope.GetIsolate())->GetScriptWrappableVisitor();

  DeathAwareScriptWrappable* base_wrapper = DeathAwareScriptWrappable::Create();
  DeathAwareScriptWrappable* mixin_wrapper =
      DeathAwareScriptWrappable::Create();
  Base* base = Base::Create(base_wrapper, mixin_wrapper);
  Mixin* mixin = static_cast<Mixin*>(base);

  HeapObjectHeader* base_header = HeapObjectHeader::FromPayload(base);
  EXPECT_FALSE(base_header->IsWrapperHeaderMarked());

  // Make sure that mixin does not point to the object header.
  EXPECT_NE(static_cast<void*>(base), static_cast<void*>(mixin));

  visitor->TracePrologue();

  EXPECT_TRUE(visitor->MarkingDeque()->IsEmpty());

  // TraceWrapperMember itself is not required to live in an Oilpan object.
  TraceWrapperMember<Mixin> mixin_handle = mixin;
  EXPECT_TRUE(base_header->IsWrapperHeaderMarked());
  EXPECT_FALSE(visitor->MarkingDeque()->IsEmpty());
  EXPECT_TRUE(DequeContains(*visitor->MarkingDeque(), mixin));

  visitor->AdvanceTracing(
      0, v8::EmbedderHeapTracer::AdvanceTracingActions(
             v8::EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION));
  EXPECT_EQ(visitor->MarkingDeque()->size(), 0ul);
  EXPECT_TRUE(base_header->IsWrapperHeaderMarked());
  EXPECT_TRUE(
      HeapObjectHeader::FromPayload(base_wrapper)->IsWrapperHeaderMarked());
  EXPECT_TRUE(
      HeapObjectHeader::FromPayload(mixin_wrapper)->IsWrapperHeaderMarked());

  mixin_handle = nullptr;
  visitor->AbortTracing();
}

}  // namespace blink
