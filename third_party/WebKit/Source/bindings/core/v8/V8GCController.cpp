/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#include "bindings/core/v8/V8GCController.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include "bindings/core/v8/RetainedDOMInfo.h"
#include "bindings/core/v8/V8AbstractEventListener.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/dom/Attr.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/bindings/ActiveScriptWrappable.h"
#include "platform/bindings/ScriptWrappableVisitor.h"
#include "platform/bindings/WrapperTypeInfo.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/allocator/Partitions.h"
#include "public/platform/BlameContext.h"
#include "public/platform/Platform.h"

namespace blink {

Node* V8GCController::OpaqueRootForGC(v8::Isolate*, Node* node) {
  DCHECK(node);
  if (node->isConnected()) {
    Document& document = node->GetDocument();
    if (HTMLImportsController* controller = document.ImportsController())
      return controller->Master();
    return &document;
  }

  if (node->IsAttributeNode()) {
    Node* owner_element = ToAttr(node)->ownerElement();
    if (!owner_element)
      return node;
    node = owner_element;
  }

  while (Node* parent = node->ParentOrShadowHostOrTemplateHostNode())
    node = parent;

  return node;
}

class MinorGCUnmodifiedWrapperVisitor : public v8::PersistentHandleVisitor {
 public:
  explicit MinorGCUnmodifiedWrapperVisitor(v8::Isolate* isolate)
      : isolate_(isolate) {}

  void VisitPersistentHandle(v8::Persistent<v8::Value>* value,
                             uint16_t class_id) override {
    if (class_id != WrapperTypeInfo::kNodeClassId &&
        class_id != WrapperTypeInfo::kObjectClassId) {
      return;
    }

    // MinorGC does not collect objects because it may be expensive to
    // update references during minorGC
    if (class_id == WrapperTypeInfo::kObjectClassId) {
      v8::Persistent<v8::Object>::Cast(*value).MarkActive();
      return;
    }

    v8::Local<v8::Object> wrapper = v8::Local<v8::Object>::New(
        isolate_, v8::Persistent<v8::Object>::Cast(*value));
    DCHECK(V8DOMWrapper::HasInternalFieldsSet(wrapper));
    if (ToWrapperTypeInfo(wrapper)->IsActiveScriptWrappable() &&
        ToScriptWrappable(wrapper)->HasPendingActivity()) {
      v8::Persistent<v8::Object>::Cast(*value).MarkActive();
      return;
    }

    if (class_id == WrapperTypeInfo::kNodeClassId) {
      DCHECK(V8Node::hasInstance(wrapper, isolate_));
      Node* node = V8Node::toImpl(wrapper);
      if (node->HasEventListeners()) {
        v8::Persistent<v8::Object>::Cast(*value).MarkActive();
        return;
      }
      // FIXME: Remove the special handling for SVG elements.
      // We currently can't collect SVG Elements from minor gc, as we have
      // strong references from SVG property tear-offs keeping context SVG
      // element alive.
      if (node->IsSVGElement()) {
        v8::Persistent<v8::Object>::Cast(*value).MarkActive();
        return;
      }
    }
  }

 private:
  v8::Isolate* isolate_;
};

class HeapSnaphotWrapperVisitor : public ScriptWrappableVisitor,
                                  public v8::PersistentHandleVisitor {
 public:
  explicit HeapSnaphotWrapperVisitor(v8::Isolate* isolate)
      : ScriptWrappableVisitor(isolate),
        current_parent_(nullptr),
        only_trace_single_level_(false),
        first_script_wrappable_traced_(false) {
    DCHECK(IsMainThread());
  }

  // Collect interesting V8 roots for the heap snapshot. Currently these are
  // DOM nodes.
  void CollectV8Roots() { isolate_->VisitHandlesWithClassIds(this); }

  void VisitPersistentHandle(v8::Persistent<v8::Value>* value,
                             uint16_t class_id) override {
    if (class_id != WrapperTypeInfo::kNodeClassId)
      return;

    DCHECK(!value->IsIndependent());
    v8::Local<v8::Object> wrapper = v8::Local<v8::Object>::New(
        isolate_, v8::Persistent<v8::Object>::Cast(*value));
    DCHECK(V8Node::hasInstance(wrapper, isolate_));
    Node* node = V8Node::toImpl(wrapper);
    Node* root = V8GCController::OpaqueRootForGC(isolate_, node);
    nodes_requiring_tracing_[root].push_back(node);
  }

  // Trace through the blink heap to find all V8 wrappers reachable from
  // ActiveScriptWrappables. Also collect retainer edges on the way.
  void TracePendingActivities() {
    CHECK(found_v8_wrappers_.empty());
    current_parent_ = nullptr;

    TracePrologue();
    ActiveScriptWrappableBase::TraceActiveScriptWrappables(isolate_, this);
    AdvanceTracing(
        0,
        v8::EmbedderHeapTracer::AdvanceTracingActions(
            v8::EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION));
    // Abort instead of Epilogue as we want to finish synchronously.
    AbortTracing();

    groups_.push_back(
        std::make_pair(new SuspendableObjectsInfo(found_v8_wrappers_.size()),
                       std::move(found_v8_wrappers_)));
  }

  // Trace through the blink heap to find all V8 wrappers reachable from any
  // of the collected roots. Also collect retainer edges on the way.
  void TraceV8Roots() {
    for (auto& group_pair : nodes_requiring_tracing_) {
      v8::HeapProfiler::RetainerChildren group_children;
      for (auto& node : group_pair.second) {
        // Ignore the actual wrappers reachable as we only want to create
        // groups of DOM nodes. The method is still used to collect edges
        // though.
        FindV8WrappersDirectlyReachableFrom(node);
        group_children.insert(PersistentForWrappable(node));
      }
      groups_.push_back(std::make_pair(new RetainedDOMInfo(group_pair.first),
                                       std::move(group_children)));
    }
  }

  v8::HeapProfiler::RetainerEdges Edges() { return std::move(edges_); }
  v8::HeapProfiler::RetainerGroups Groups() { return std::move(groups_); }

  void MarkWrappersInAllWorlds(
      const ScriptWrappable* traceable) const override {
    // Only mark the main thread wrapper as we cannot properly intercept
    // DOMWrapperMap::markWrapper. This means that edges from the isolated
    // worlds are missing in the snapshot.
    traceable->MarkWrapper(this);
  }

  void MarkWrapper(const v8::PersistentBase<v8::Value>* value) const override {
    if (current_parent_ && current_parent_ != value)
      edges_.push_back(std::make_pair(current_parent_, value));
    found_v8_wrappers_.insert(value);
  }

  void DispatchTraceWrappers(const TraceWrapperBase* traceable) const override {
    if (!only_trace_single_level_ || !traceable->IsScriptWrappable() ||
        !reinterpret_cast<const ScriptWrappable*>(traceable)
             ->ContainsWrapper() ||
        !first_script_wrappable_traced_) {
      first_script_wrappable_traced_ = true;
      traceable->TraceWrappers(this);
    }
  }

 private:
  inline v8::PersistentBase<v8::Value>* PersistentForWrappable(
      ScriptWrappable* wrappable) {
    return &v8::Persistent<v8::Value>::Cast(*wrappable->RawMainWorldWrapper());
  }

  v8::HeapProfiler::RetainerChildren FindV8WrappersDirectlyReachableFrom(
      Node* traceable) {
    CHECK(found_v8_wrappers_.empty());
    WTF::AutoReset<bool> scope(&only_trace_single_level_, true);
    first_script_wrappable_traced_ = false;
    current_parent_ = PersistentForWrappable(traceable);

    TracePrologue();
    traceable->GetWrapperTypeInfo()->TraceWrappers(this, traceable);
    AdvanceTracing(
        0,
        v8::EmbedderHeapTracer::AdvanceTracingActions(
            v8::EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION));
    // Abort instead of Epilogue as we want to finish synchronously.
    AbortTracing();

    return std::move(found_v8_wrappers_);
  }

  // Input obtained from |VisitPersistentHandle|.
  std::unordered_map<Node*, std::vector<Node*>> nodes_requiring_tracing_;

  // Temporaries used for tracing a single Node.
  const v8::PersistentBase<v8::Value>* current_parent_;
  bool only_trace_single_level_;
  mutable bool first_script_wrappable_traced_;
  mutable v8::HeapProfiler::RetainerChildren found_v8_wrappers_;

  // Out variables
  mutable v8::HeapProfiler::RetainerEdges edges_;
  mutable v8::HeapProfiler::RetainerGroups groups_;
};

// The function |getRetainerInfos| processing all handles on the blink heap,
// logically grouping together DOM trees (attached and detached) and pending
// activities, while at the same time finding parent to child relationships
// for non-Node wrappers. Since we are processing *all* handles there is no
// way we miss out on a handle. V8 will figure out the liveness information
// for the provided information itself.
v8::HeapProfiler::RetainerInfos V8GCController::GetRetainerInfos(
    v8::Isolate* isolate) {
  V8PerIsolateData::TemporaryScriptWrappableVisitorScope scope(
      isolate, std::unique_ptr<HeapSnaphotWrapperVisitor>(
                   new HeapSnaphotWrapperVisitor(isolate)));

  HeapSnaphotWrapperVisitor* tracer =
      reinterpret_cast<HeapSnaphotWrapperVisitor*>(scope.CurrentVisitor());
  tracer->CollectV8Roots();
  tracer->TraceV8Roots();
  tracer->TracePendingActivities();
  return v8::HeapProfiler::RetainerInfos{tracer->Groups(), tracer->Edges()};
}

static unsigned long long UsedHeapSize(v8::Isolate* isolate) {
  v8::HeapStatistics heap_statistics;
  isolate->GetHeapStatistics(&heap_statistics);
  return heap_statistics.used_heap_size();
}

namespace {

void VisitWeakHandlesForMinorGC(v8::Isolate* isolate) {
  MinorGCUnmodifiedWrapperVisitor visitor(isolate);
  isolate->VisitWeakHandles(&visitor);
}

}  // namespace

void V8GCController::GcPrologue(v8::Isolate* isolate,
                                v8::GCType type,
                                v8::GCCallbackFlags flags) {
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kGcPrologue);
  if (IsMainThread())
    ScriptForbiddenScope::Enter();

  // Attribute garbage collection to the all frames instead of a specific
  // frame.
  if (BlameContext* blame_context =
          Platform::Current()->GetTopLevelBlameContext())
    blame_context->Enter();

  // TODO(haraken): A GC callback is not allowed to re-enter V8. This means
  // that it's unsafe to run Oilpan's GC in the GC callback because it may
  // run finalizers that call into V8. To avoid the risk, we should post
  // a task to schedule the Oilpan's GC.
  // (In practice, there is no finalizer that calls into V8 and thus is safe.)

  v8::HandleScope scope(isolate);
  switch (type) {
    case v8::kGCTypeScavenge:
      if (ThreadState::Current())
        ThreadState::Current()->WillStartV8GC(BlinkGC::kV8MinorGC);

      TRACE_EVENT_BEGIN1("devtools.timeline,v8", "MinorGC",
                         "usedHeapSizeBefore", UsedHeapSize(isolate));
      VisitWeakHandlesForMinorGC(isolate);
      break;
    case v8::kGCTypeMarkSweepCompact:
      if (ThreadState::Current())
        ThreadState::Current()->WillStartV8GC(BlinkGC::kV8MajorGC);

      TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC",
                         "usedHeapSizeBefore", UsedHeapSize(isolate), "type",
                         "atomic pause");
      break;
    case v8::kGCTypeIncrementalMarking:
      if (ThreadState::Current())
        ThreadState::Current()->WillStartV8GC(BlinkGC::kV8MajorGC);

      TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC",
                         "usedHeapSizeBefore", UsedHeapSize(isolate), "type",
                         "incremental marking");
      break;
    case v8::kGCTypeProcessWeakCallbacks:
      TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC",
                         "usedHeapSizeBefore", UsedHeapSize(isolate), "type",
                         "weak processing");
      break;
    default:
      NOTREACHED();
  }
}

namespace {

void UpdateCollectedPhantomHandles(v8::Isolate* isolate) {
  ThreadHeapStats& heap_stats = ThreadState::Current()->Heap().HeapStats();
  size_t count = isolate->NumberOfPhantomHandleResetsSinceLastCall();
  heap_stats.DecreaseWrapperCount(count);
  heap_stats.IncreaseCollectedWrapperCount(count);
}

}  // namespace

void V8GCController::GcEpilogue(v8::Isolate* isolate,
                                v8::GCType type,
                                v8::GCCallbackFlags flags) {
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kGcEpilogue);
  UpdateCollectedPhantomHandles(isolate);
  switch (type) {
    case v8::kGCTypeScavenge:
      TRACE_EVENT_END1("devtools.timeline,v8", "MinorGC", "usedHeapSizeAfter",
                       UsedHeapSize(isolate));
      // TODO(haraken): Remove this. See the comment in gcPrologue.
      if (ThreadState::Current())
        ThreadState::Current()->ScheduleV8FollowupGCIfNeeded(
            BlinkGC::kV8MinorGC);
      break;
    case v8::kGCTypeMarkSweepCompact:
      TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter",
                       UsedHeapSize(isolate));
      if (ThreadState::Current())
        ThreadState::Current()->ScheduleV8FollowupGCIfNeeded(
            BlinkGC::kV8MajorGC);
      break;
    case v8::kGCTypeIncrementalMarking:
      TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter",
                       UsedHeapSize(isolate));
      break;
    case v8::kGCTypeProcessWeakCallbacks:
      TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter",
                       UsedHeapSize(isolate));
      break;
    default:
      NOTREACHED();
  }

  if (IsMainThread())
    ScriptForbiddenScope::Exit();

  if (BlameContext* blame_context =
          Platform::Current()->GetTopLevelBlameContext())
    blame_context->Leave();

  ThreadState* current_thread_state = ThreadState::Current();
  if (current_thread_state && !current_thread_state->IsGCForbidden()) {
    // v8::kGCCallbackFlagForced forces a Blink heap garbage collection
    // when a garbage collection was forced from V8. This is either used
    // for tests that force GCs from JavaScript to verify that objects die
    // when expected.
    if (flags & v8::kGCCallbackFlagForced) {
      // This single GC is not enough for two reasons:
      //   (1) The GC is not precise because the GC scans on-stack pointers
      //       conservatively.
      //   (2) One GC is not enough to break a chain of persistent handles. It's
      //       possible that some heap allocated objects own objects that
      //       contain persistent handles pointing to other heap allocated
      //       objects. To break the chain, we need multiple GCs.
      //
      // Regarding (1), we force a precise GC at the end of the current event
      // loop. So if you want to collect all garbage, you need to wait until the
      // next event loop.  Regarding (2), it would be OK in practice to trigger
      // only one GC per gcEpilogue, because GCController.collectAll() forces
      // multiple V8's GC.
      current_thread_state->CollectGarbage(BlinkGC::kHeapPointersOnStack,
                                           BlinkGC::kGCWithSweep,
                                           BlinkGC::kForcedGC);

      // Forces a precise GC at the end of the current event loop.
      CHECK(!current_thread_state->IsInGC());
      current_thread_state->SetGCState(ThreadState::kFullGCScheduled);
    }

    // v8::kGCCallbackFlagCollectAllAvailableGarbage is used when V8 handles
    // low memory notifications.
    if ((flags & v8::kGCCallbackFlagCollectAllAvailableGarbage) ||
        (flags & v8::kGCCallbackFlagCollectAllExternalMemory)) {
      // This single GC is not enough. See the above comment.
      current_thread_state->CollectGarbage(BlinkGC::kHeapPointersOnStack,
                                           BlinkGC::kGCWithSweep,
                                           BlinkGC::kForcedGC);

      // The conservative GC might have left floating garbage. Schedule
      // precise GC to ensure that we collect all available garbage.
      current_thread_state->SchedulePreciseGC();
    }

    // Schedules a precise GC for the next idle time period.
    if (flags & v8::kGCCallbackScheduleIdleGarbageCollection) {
      current_thread_state->ScheduleIdleGC();
    }
  }

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorUpdateCountersEvent::Data());
}

void V8GCController::CollectGarbage(v8::Isolate* isolate, bool only_minor_gc) {
  v8::HandleScope handle_scope(isolate);
  RefPtr<ScriptState> script_state = ScriptState::Create(
      v8::Context::New(isolate),
      DOMWrapperWorld::Create(isolate,
                              DOMWrapperWorld::WorldType::kGarbageCollector));
  ScriptState::Scope scope(script_state.Get());
  StringBuilder builder;
  builder.Append("if (gc) gc(");
  builder.Append(only_minor_gc ? "true" : "false");
  builder.Append(")");
  V8ScriptRunner::CompileAndRunInternalScript(
      V8String(isolate, builder.ToString()), isolate);
  script_state->DisposePerContextData();
}

void V8GCController::CollectAllGarbageForTesting(v8::Isolate* isolate) {
  for (unsigned i = 0; i < 5; i++)
    isolate->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection);
}

class DOMWrapperTracer : public v8::PersistentHandleVisitor {
 public:
  explicit DOMWrapperTracer(Visitor* visitor) : visitor_(visitor) {
    DCHECK(visitor_);
  }

  void VisitPersistentHandle(v8::Persistent<v8::Value>* value,
                             uint16_t class_id) override {
    if (class_id != WrapperTypeInfo::kNodeClassId &&
        class_id != WrapperTypeInfo::kObjectClassId)
      return;

    const v8::Persistent<v8::Object>& wrapper =
        v8::Persistent<v8::Object>::Cast(*value);

    if (ScriptWrappable* script_wrappable = ToScriptWrappable(wrapper))
      ToWrapperTypeInfo(wrapper)->Trace(visitor_, script_wrappable);
  }

 private:
  Visitor* visitor_;
};

void V8GCController::TraceDOMWrappers(v8::Isolate* isolate, Visitor* visitor) {
  DOMWrapperTracer tracer(visitor);
  isolate->VisitHandlesWithClassIds(&tracer);
}

class PendingActivityVisitor : public v8::PersistentHandleVisitor {
 public:
  PendingActivityVisitor(v8::Isolate* isolate,
                         ExecutionContext* execution_context)
      : isolate_(isolate),
        execution_context_(execution_context),
        pending_activity_found_(false) {}

  void VisitPersistentHandle(v8::Persistent<v8::Value>* value,
                             uint16_t class_id) override {
    // If we have already found any wrapper that has a pending activity,
    // we don't need to check other wrappers.
    if (pending_activity_found_)
      return;

    if (class_id != WrapperTypeInfo::kNodeClassId &&
        class_id != WrapperTypeInfo::kObjectClassId)
      return;

    v8::Local<v8::Object> wrapper = v8::Local<v8::Object>::New(
        isolate_, v8::Persistent<v8::Object>::Cast(*value));
    DCHECK(V8DOMWrapper::HasInternalFieldsSet(wrapper));
    // The ExecutionContext check is heavy, so it should be done at the last.
    if (ToWrapperTypeInfo(wrapper)->IsActiveScriptWrappable() &&
        ToScriptWrappable(wrapper)->HasPendingActivity()) {
      // See the comment in MajorGCWrapperVisitor::VisitPersistentHandle.
      ExecutionContext* context =
          ToExecutionContext(wrapper->CreationContext());
      if (context == execution_context_ && context &&
          !context->IsContextDestroyed())
        pending_activity_found_ = true;
    }
  }

  bool PendingActivityFound() const { return pending_activity_found_; }

 private:
  v8::Isolate* isolate_;
  Persistent<ExecutionContext> execution_context_;
  bool pending_activity_found_;
};

bool V8GCController::HasPendingActivity(v8::Isolate* isolate,
                                        ExecutionContext* execution_context) {
  // V8GCController::hasPendingActivity is used only when a worker checks if
  // the worker contains any wrapper that has pending activities.
  DCHECK(!IsMainThread());

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, scan_pending_activity_histogram,
      ("Blink.ScanPendingActivityDuration", 1, 1000, 50));
  double start_time = WTF::CurrentTimeMS();
  v8::HandleScope scope(isolate);
  PendingActivityVisitor visitor(isolate, execution_context);
  ToIsolate(execution_context)->VisitHandlesWithClassIds(&visitor);
  scan_pending_activity_histogram.Count(
      static_cast<int>(WTF::CurrentTimeMS() - start_time));
  return visitor.PendingActivityFound();
}

}  // namespace blink
