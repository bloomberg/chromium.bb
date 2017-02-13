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

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/RetainedDOMInfo.h"
#include "bindings/core/v8/ScriptWrappableVisitor.h"
#include "bindings/core/v8/V8AbstractEventListener.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/WrapperTypeInfo.h"
#include "core/dom/Attr.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "platform/Histogram.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "public/platform/BlameContext.h"
#include "public/platform/Platform.h"
#include "wtf/Vector.h"
#include "wtf/allocator/Partitions.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace blink {

Node* V8GCController::opaqueRootForGC(v8::Isolate*, Node* node) {
  ASSERT(node);
  if (node->isConnected()) {
    Document& document = node->document();
    if (HTMLImportsController* controller = document.importsController())
      return controller->master();
    return &document;
  }

  if (node->isAttributeNode()) {
    Node* ownerElement = toAttr(node)->ownerElement();
    if (!ownerElement)
      return node;
    node = ownerElement;
  }

  while (Node* parent = node->parentOrShadowHostOrTemplateHostNode())
    node = parent;

  return node;
}

class MinorGCUnmodifiedWrapperVisitor : public v8::PersistentHandleVisitor {
 public:
  explicit MinorGCUnmodifiedWrapperVisitor(v8::Isolate* isolate)
      : m_isolate(isolate) {}

  void VisitPersistentHandle(v8::Persistent<v8::Value>* value,
                             uint16_t classId) override {
    if (classId != WrapperTypeInfo::NodeClassId &&
        classId != WrapperTypeInfo::ObjectClassId) {
      return;
    }

    // MinorGC does not collect objects because it may be expensive to
    // update references during minorGC
    if (classId == WrapperTypeInfo::ObjectClassId) {
      v8::Persistent<v8::Object>::Cast(*value).MarkActive();
      return;
    }

    v8::Local<v8::Object> wrapper = v8::Local<v8::Object>::New(
        m_isolate, v8::Persistent<v8::Object>::Cast(*value));
    ASSERT(V8DOMWrapper::hasInternalFieldsSet(wrapper));
    if (toWrapperTypeInfo(wrapper)->isActiveScriptWrappable() &&
        toScriptWrappable(wrapper)->hasPendingActivity()) {
      v8::Persistent<v8::Object>::Cast(*value).MarkActive();
      return;
    }

    if (classId == WrapperTypeInfo::NodeClassId) {
      ASSERT(V8Node::hasInstance(wrapper, m_isolate));
      Node* node = V8Node::toImpl(wrapper);
      if (node->hasEventListeners()) {
        v8::Persistent<v8::Object>::Cast(*value).MarkActive();
        return;
      }
      // FIXME: Remove the special handling for SVG elements.
      // We currently can't collect SVG Elements from minor gc, as we have
      // strong references from SVG property tear-offs keeping context SVG
      // element alive.
      if (node->isSVGElement()) {
        v8::Persistent<v8::Object>::Cast(*value).MarkActive();
        return;
      }
    }
  }

 private:
  v8::Isolate* m_isolate;
};

class HeapSnaphotWrapperVisitor : public ScriptWrappableVisitor,
                                  public v8::PersistentHandleVisitor {
 public:
  explicit HeapSnaphotWrapperVisitor(v8::Isolate* isolate)
      : ScriptWrappableVisitor(isolate),
        m_currentParent(nullptr),
        m_onlyTraceSingleLevel(false),
        m_firstScriptWrappableTraced(false) {
    DCHECK(isMainThread());
  }

  // Collect interesting V8 roots for the heap snapshot. Currently these are
  // DOM nodes.
  void collectV8Roots() { m_isolate->VisitHandlesWithClassIds(this); }

  void VisitPersistentHandle(v8::Persistent<v8::Value>* value,
                             uint16_t classId) override {
    if (classId != WrapperTypeInfo::NodeClassId)
      return;

    DCHECK(!value->IsIndependent());
    v8::Local<v8::Object> wrapper = v8::Local<v8::Object>::New(
        m_isolate, v8::Persistent<v8::Object>::Cast(*value));
    DCHECK(V8Node::hasInstance(wrapper, m_isolate));
    Node* node = V8Node::toImpl(wrapper);
    Node* root = V8GCController::opaqueRootForGC(m_isolate, node);
    m_nodesRequiringTracing[root].push_back(node);
  }

  // Trace through the blink heap to find all V8 wrappers reachable from
  // ActiveScriptWrappables. Also collect retainer edges on the way.
  void tracePendingActivities() {
    CHECK(m_foundV8Wrappers.empty());
    m_currentParent = nullptr;

    TracePrologue();
    ActiveScriptWrappableBase::traceActiveScriptWrappables(m_isolate, this);
    AdvanceTracing(
        0,
        v8::EmbedderHeapTracer::AdvanceTracingActions(
            v8::EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION));
    // Abort instead of Epilogue as we want to finish synchronously.
    AbortTracing();

    m_groups.push_back(
        std::make_pair(new SuspendableObjectsInfo(m_foundV8Wrappers.size()),
                       std::move(m_foundV8Wrappers)));
  }

  // Trace through the blink heap to find all V8 wrappers reachable from any
  // of the collected roots. Also collect retainer edges on the way.
  void traceV8Roots() {
    for (auto& groupPair : m_nodesRequiringTracing) {
      v8::HeapProfiler::RetainerChildren groupChildren;
      for (auto& node : groupPair.second) {
        // Ignore the actual wrappers reachable as we only want to create
        // groups of DOM nodes. The method is still used to collect edges
        // though.
        findV8WrappersDirectlyReachableFrom(node);
        groupChildren.insert(persistentForWrappable(node));
      }
      m_groups.push_back(std::make_pair(new RetainedDOMInfo(groupPair.first),
                                        std::move(groupChildren)));
    }
  }

  v8::HeapProfiler::RetainerEdges edges() { return std::move(m_edges); }
  v8::HeapProfiler::RetainerGroups groups() { return std::move(m_groups); }

  void markWrappersInAllWorlds(
      const ScriptWrappable* traceable) const override {
    // Only mark the main thread wrapper as we cannot properly intercept
    // DOMWrapperMap::markWrapper. This means that edges from the isolated
    // worlds are missing in the snapshot.
    traceable->markWrapper(this);
  }

  void markWrapper(const v8::PersistentBase<v8::Value>* value) const override {
    if (m_currentParent && m_currentParent != value)
      m_edges.push_back(std::make_pair(m_currentParent, value));
    m_foundV8Wrappers.insert(value);
  }

  void dispatchTraceWrappers(const TraceWrapperBase* traceable) const override {
    if (!m_onlyTraceSingleLevel || !traceable->isScriptWrappable() ||
        !reinterpret_cast<const ScriptWrappable*>(traceable)
             ->containsWrapper() ||
        !m_firstScriptWrappableTraced) {
      m_firstScriptWrappableTraced = true;
      traceable->traceWrappers(this);
    }
  }

 private:
  inline v8::PersistentBase<v8::Value>* persistentForWrappable(
      ScriptWrappable* wrappable) {
    return &v8::Persistent<v8::Value>::Cast(*wrappable->rawMainWorldWrapper());
  }

  v8::HeapProfiler::RetainerChildren findV8WrappersDirectlyReachableFrom(
      Node* traceable) {
    CHECK(m_foundV8Wrappers.empty());
    WTF::AutoReset<bool> scope(&m_onlyTraceSingleLevel, true);
    m_firstScriptWrappableTraced = false;
    m_currentParent = persistentForWrappable(traceable);

    TracePrologue();
    traceable->wrapperTypeInfo()->traceWrappers(this, traceable);
    AdvanceTracing(
        0,
        v8::EmbedderHeapTracer::AdvanceTracingActions(
            v8::EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION));
    // Abort instead of Epilogue as we want to finish synchronously.
    AbortTracing();

    return std::move(m_foundV8Wrappers);
  }

  // Input obtained from |VisitPersistentHandle|.
  std::unordered_map<Node*, std::vector<Node*>> m_nodesRequiringTracing;

  // Temporaries used for tracing a single Node.
  const v8::PersistentBase<v8::Value>* m_currentParent;
  bool m_onlyTraceSingleLevel;
  mutable bool m_firstScriptWrappableTraced;
  mutable v8::HeapProfiler::RetainerChildren m_foundV8Wrappers;

  // Out variables
  mutable v8::HeapProfiler::RetainerEdges m_edges;
  mutable v8::HeapProfiler::RetainerGroups m_groups;
};

// The function |getRetainerInfos| processing all handles on the blink heap,
// logically grouping together DOM trees (attached and detached) and pending
// activities, while at the same time finding parent to child relationships
// for non-Node wrappers. Since we are processing *all* handles there is no
// way we miss out on a handle. V8 will figure out the liveness information
// for the provided information itself.
v8::HeapProfiler::RetainerInfos V8GCController::getRetainerInfos(
    v8::Isolate* isolate) {
  V8PerIsolateData::TemporaryScriptWrappableVisitorScope scope(
      isolate, std::unique_ptr<HeapSnaphotWrapperVisitor>(
                   new HeapSnaphotWrapperVisitor(isolate)));

  HeapSnaphotWrapperVisitor* tracer =
      reinterpret_cast<HeapSnaphotWrapperVisitor*>(scope.currentVisitor());
  tracer->collectV8Roots();
  tracer->traceV8Roots();
  tracer->tracePendingActivities();
  return v8::HeapProfiler::RetainerInfos{tracer->groups(), tracer->edges()};
}

static unsigned long long usedHeapSize(v8::Isolate* isolate) {
  v8::HeapStatistics heapStatistics;
  isolate->GetHeapStatistics(&heapStatistics);
  return heapStatistics.used_heap_size();
}

namespace {

void visitWeakHandlesForMinorGC(v8::Isolate* isolate) {
  MinorGCUnmodifiedWrapperVisitor visitor(isolate);
  isolate->VisitWeakHandles(&visitor);
}

}  // namespace

void V8GCController::gcPrologue(v8::Isolate* isolate,
                                v8::GCType type,
                                v8::GCCallbackFlags flags) {
  if (isMainThread())
    ScriptForbiddenScope::enter();

  // Attribute garbage collection to the all frames instead of a specific
  // frame.
  if (BlameContext* blameContext = Platform::current()->topLevelBlameContext())
    blameContext->Enter();

  // TODO(haraken): A GC callback is not allowed to re-enter V8. This means
  // that it's unsafe to run Oilpan's GC in the GC callback because it may
  // run finalizers that call into V8. To avoid the risk, we should post
  // a task to schedule the Oilpan's GC.
  // (In practice, there is no finalizer that calls into V8 and thus is safe.)

  v8::HandleScope scope(isolate);
  switch (type) {
    case v8::kGCTypeScavenge:
      if (ThreadState::current())
        ThreadState::current()->willStartV8GC(BlinkGC::V8MinorGC);

      TRACE_EVENT_BEGIN1("devtools.timeline,v8", "MinorGC",
                         "usedHeapSizeBefore", usedHeapSize(isolate));
      visitWeakHandlesForMinorGC(isolate);
      break;
    case v8::kGCTypeMarkSweepCompact:
      if (ThreadState::current())
        ThreadState::current()->willStartV8GC(BlinkGC::V8MajorGC);

      TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC",
                         "usedHeapSizeBefore", usedHeapSize(isolate), "type",
                         "atomic pause");
      break;
    case v8::kGCTypeIncrementalMarking:
      if (ThreadState::current())
        ThreadState::current()->willStartV8GC(BlinkGC::V8MajorGC);

      TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC",
                         "usedHeapSizeBefore", usedHeapSize(isolate), "type",
                         "incremental marking");
      break;
    case v8::kGCTypeProcessWeakCallbacks:
      TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC",
                         "usedHeapSizeBefore", usedHeapSize(isolate), "type",
                         "weak processing");
      break;
    default:
      ASSERT_NOT_REACHED();
  }
}

namespace {

void UpdateCollectedPhantomHandles(v8::Isolate* isolate) {
  ThreadHeapStats& heapStats = ThreadState::current()->heap().heapStats();
  size_t count = isolate->NumberOfPhantomHandleResetsSinceLastCall();
  heapStats.decreaseWrapperCount(count);
  heapStats.increaseCollectedWrapperCount(count);
}

}  // namespace

void V8GCController::gcEpilogue(v8::Isolate* isolate,
                                v8::GCType type,
                                v8::GCCallbackFlags flags) {
  UpdateCollectedPhantomHandles(isolate);
  switch (type) {
    case v8::kGCTypeScavenge:
      TRACE_EVENT_END1("devtools.timeline,v8", "MinorGC", "usedHeapSizeAfter",
                       usedHeapSize(isolate));
      // TODO(haraken): Remove this. See the comment in gcPrologue.
      if (ThreadState::current())
        ThreadState::current()->scheduleV8FollowupGCIfNeeded(
            BlinkGC::V8MinorGC);
      break;
    case v8::kGCTypeMarkSweepCompact:
      TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter",
                       usedHeapSize(isolate));
      if (ThreadState::current())
        ThreadState::current()->scheduleV8FollowupGCIfNeeded(
            BlinkGC::V8MajorGC);
      break;
    case v8::kGCTypeIncrementalMarking:
      TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter",
                       usedHeapSize(isolate));
      break;
    case v8::kGCTypeProcessWeakCallbacks:
      TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter",
                       usedHeapSize(isolate));
      break;
    default:
      ASSERT_NOT_REACHED();
  }

  if (isMainThread())
    ScriptForbiddenScope::exit();

  if (BlameContext* blameContext = Platform::current()->topLevelBlameContext())
    blameContext->Leave();

  ThreadState* currentThreadState = ThreadState::current();
  if (currentThreadState && !currentThreadState->isGCForbidden()) {
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
      currentThreadState->collectGarbage(BlinkGC::HeapPointersOnStack,
                                         BlinkGC::GCWithSweep,
                                         BlinkGC::ForcedGC);

      // Forces a precise GC at the end of the current event loop.
      RELEASE_ASSERT(!currentThreadState->isInGC());
      currentThreadState->setGCState(ThreadState::FullGCScheduled);
    }

    // v8::kGCCallbackFlagCollectAllAvailableGarbage is used when V8 handles
    // low memory notifications.
    if ((flags & v8::kGCCallbackFlagCollectAllAvailableGarbage) ||
        (flags & v8::kGCCallbackFlagCollectAllExternalMemory)) {
      // This single GC is not enough. See the above comment.
      currentThreadState->collectGarbage(BlinkGC::HeapPointersOnStack,
                                         BlinkGC::GCWithSweep,
                                         BlinkGC::ForcedGC);

      // The conservative GC might have left floating garbage. Schedule
      // precise GC to ensure that we collect all available garbage.
      currentThreadState->schedulePreciseGC();
    }
  }

  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data",
                       InspectorUpdateCountersEvent::data());
}

void V8GCController::collectGarbage(v8::Isolate* isolate, bool onlyMinorGC) {
  v8::HandleScope handleScope(isolate);
  RefPtr<ScriptState> scriptState = ScriptState::create(
      v8::Context::New(isolate), DOMWrapperWorld::create(isolate));
  ScriptState::Scope scope(scriptState.get());
  StringBuilder builder;
  builder.append("if (gc) gc(");
  builder.append(onlyMinorGC ? "true" : "false");
  builder.append(")");
  V8ScriptRunner::compileAndRunInternalScript(
      v8String(isolate, builder.toString()), isolate);
  scriptState->disposePerContextData();
}

void V8GCController::collectAllGarbageForTesting(v8::Isolate* isolate) {
  for (unsigned i = 0; i < 5; i++)
    isolate->RequestGarbageCollectionForTesting(
        v8::Isolate::kFullGarbageCollection);
}

class DOMWrapperTracer : public v8::PersistentHandleVisitor {
 public:
  explicit DOMWrapperTracer(Visitor* visitor) : m_visitor(visitor) {}

  void VisitPersistentHandle(v8::Persistent<v8::Value>* value,
                             uint16_t classId) override {
    if (classId != WrapperTypeInfo::NodeClassId &&
        classId != WrapperTypeInfo::ObjectClassId)
      return;

    const v8::Persistent<v8::Object>& wrapper =
        v8::Persistent<v8::Object>::Cast(*value);

    if (m_visitor)
      toWrapperTypeInfo(wrapper)->trace(m_visitor, toScriptWrappable(wrapper));
  }

 private:
  Visitor* m_visitor;
};

void V8GCController::traceDOMWrappers(v8::Isolate* isolate, Visitor* visitor) {
  DOMWrapperTracer tracer(visitor);
  isolate->VisitHandlesWithClassIds(&tracer);
}

class PendingActivityVisitor : public v8::PersistentHandleVisitor {
 public:
  PendingActivityVisitor(v8::Isolate* isolate,
                         ExecutionContext* executionContext)
      : m_isolate(isolate),
        m_executionContext(executionContext),
        m_pendingActivityFound(false) {}

  void VisitPersistentHandle(v8::Persistent<v8::Value>* value,
                             uint16_t classId) override {
    // If we have already found any wrapper that has a pending activity,
    // we don't need to check other wrappers.
    if (m_pendingActivityFound)
      return;

    if (classId != WrapperTypeInfo::NodeClassId &&
        classId != WrapperTypeInfo::ObjectClassId)
      return;

    v8::Local<v8::Object> wrapper = v8::Local<v8::Object>::New(
        m_isolate, v8::Persistent<v8::Object>::Cast(*value));
    ASSERT(V8DOMWrapper::hasInternalFieldsSet(wrapper));
    // The ExecutionContext check is heavy, so it should be done at the last.
    if (toWrapperTypeInfo(wrapper)->isActiveScriptWrappable() &&
        toScriptWrappable(wrapper)->hasPendingActivity()) {
      // See the comment in MajorGCWrapperVisitor::VisitPersistentHandle.
      ExecutionContext* context =
          toExecutionContext(wrapper->CreationContext());
      if (context == m_executionContext && context &&
          !context->isContextDestroyed())
        m_pendingActivityFound = true;
    }
  }

  bool pendingActivityFound() const { return m_pendingActivityFound; }

 private:
  v8::Isolate* m_isolate;
  Persistent<ExecutionContext> m_executionContext;
  bool m_pendingActivityFound;
};

bool V8GCController::hasPendingActivity(v8::Isolate* isolate,
                                        ExecutionContext* executionContext) {
  // V8GCController::hasPendingActivity is used only when a worker checks if
  // the worker contains any wrapper that has pending activities.
  ASSERT(!isMainThread());

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, scanPendingActivityHistogram,
      new CustomCountHistogram("Blink.ScanPendingActivityDuration", 1, 1000,
                               50));
  double startTime = WTF::currentTimeMS();
  v8::HandleScope scope(isolate);
  PendingActivityVisitor visitor(isolate, executionContext);
  toIsolate(executionContext)->VisitHandlesWithClassIds(&visitor);
  scanPendingActivityHistogram.count(
      static_cast<int>(WTF::currentTimeMS() - startTime));
  return visitor.pendingActivityFound();
}

}  // namespace blink
