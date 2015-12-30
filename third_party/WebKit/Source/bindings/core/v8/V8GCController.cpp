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

#include "bindings/core/v8/RetainedDOMInfo.h"
#include "bindings/core/v8/V8AbstractEventListener.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8MutationObserver.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/WrapperTypeInfo.h"
#include "core/dom/Attr.h"
#include "core/dom/Document.h"
#include "core/dom/NodeTraversal.h"
#include "core/dom/TemplateContentDocumentFragment.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLTemplateElement.h"
#include "core/html/imports/HTMLImportsController.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/svg/SVGElement.h"
#include "platform/TraceEvent.h"
#include "wtf/Partitions.h"
#include "wtf/Vector.h"
#include <algorithm>

namespace blink {

// FIXME: This should use opaque GC roots.
static void addReferencesForNodeWithEventListeners(v8::Isolate* isolate, Node* node, const v8::Persistent<v8::Object>& wrapper)
{
    ASSERT(node->hasEventListeners());

    EventListenerIterator iterator(node);
    while (EventListener* listener = iterator.nextListener()) {
        if (listener->type() != EventListener::JSEventListenerType)
            continue;
        V8AbstractEventListener* v8listener = static_cast<V8AbstractEventListener*>(listener);
        if (!v8listener->hasExistingListenerObject())
            continue;

        isolate->SetReference(wrapper, v8::Persistent<v8::Value>::Cast(v8listener->existingListenerObjectPersistentHandle()));
    }
}

Node* V8GCController::opaqueRootForGC(v8::Isolate*, Node* node)
{
    ASSERT(node);
    // FIXME: Remove the special handling for image elements.
    // Maybe should image elements be active DOM nodes?
    // See https://code.google.com/p/chromium/issues/detail?id=164882
    if (node->inDocument() || (isHTMLImageElement(*node) && toHTMLImageElement(*node).hasPendingActivity())) {
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
        : m_isolate(isolate)
    { }

    void VisitPersistentHandle(v8::Persistent<v8::Value>* value, uint16_t classId) override
    {

        if (classId != WrapperTypeInfo::NodeClassId && classId != WrapperTypeInfo::ObjectClassId) {
            return;
        }

        // MinorGC does not collect objects because it may be expensive to
        // update references during minorGC
        if (classId == WrapperTypeInfo::ObjectClassId) {
            v8::Persistent<v8::Object>::Cast(*value).MarkActive();
            return;
        }

        v8::Local<v8::Object> wrapper = v8::Local<v8::Object>::New(m_isolate, v8::Persistent<v8::Object>::Cast(*value));
        ASSERT(V8DOMWrapper::hasInternalFieldsSet(wrapper));
        const WrapperTypeInfo* type = toWrapperTypeInfo(wrapper);
        ActiveDOMObject* activeDOMObject = type->toActiveDOMObject(wrapper);
        if (activeDOMObject && activeDOMObject->hasPendingActivity()) {
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
            // FIXME: Remove the special handling for image elements.
            // The same special handling is in V8GCController::opaqueRootForGC().
            // Maybe should image elements be active DOM nodes?
            // See https://code.google.com/p/chromium/issues/detail?id=164882
            if (isHTMLImageElement(*node) && toHTMLImageElement(*node).hasPendingActivity()) {
                v8::Persistent<v8::Object>::Cast(*value).MarkActive();
                return;
            }
            // FIXME: Remove the special handling for SVG elements.
            // We currently can't collect SVG Elements from minor gc, as we have
            // strong references from SVG property tear-offs keeping context SVG element alive.
            if (node->isSVGElement()) {
                v8::Persistent<v8::Object>::Cast(*value).MarkActive();
                return;
            }
        }
    }

private:
    v8::Isolate* m_isolate;
};

class MajorGCWrapperVisitor : public v8::PersistentHandleVisitor {
public:
    explicit MajorGCWrapperVisitor(v8::Isolate* isolate, bool constructRetainedObjectInfos)
        : m_isolate(isolate)
        , m_domObjectsWithPendingActivity(0)
        , m_liveRootGroupIdSet(false)
        , m_constructRetainedObjectInfos(constructRetainedObjectInfos)
    {
    }

    void VisitPersistentHandle(v8::Persistent<v8::Value>* value, uint16_t classId) override
    {
        if (classId != WrapperTypeInfo::NodeClassId && classId != WrapperTypeInfo::ObjectClassId)
            return;

        v8::Local<v8::Object> wrapper = v8::Local<v8::Object>::New(m_isolate, v8::Persistent<v8::Object>::Cast(*value));
        ASSERT(V8DOMWrapper::hasInternalFieldsSet(wrapper));

        if (value->IsIndependent())
            return;

        const WrapperTypeInfo* type = toWrapperTypeInfo(wrapper);

        ActiveDOMObject* activeDOMObject = type->toActiveDOMObject(wrapper);
        if (activeDOMObject && activeDOMObject->hasPendingActivity()) {
            m_isolate->SetObjectGroupId(*value, liveRootId());
            ++m_domObjectsWithPendingActivity;
        }

        if (classId == WrapperTypeInfo::NodeClassId) {
            ASSERT(V8Node::hasInstance(wrapper, m_isolate));
            Node* node = V8Node::toImpl(wrapper);
            if (node->hasEventListeners())
                addReferencesForNodeWithEventListeners(m_isolate, node, v8::Persistent<v8::Object>::Cast(*value));
            Node* root = V8GCController::opaqueRootForGC(m_isolate, node);
            m_isolate->SetObjectGroupId(*value, v8::UniqueId(reinterpret_cast<intptr_t>(root)));
            if (m_constructRetainedObjectInfos)
                m_groupsWhichNeedRetainerInfo.append(root);
        } else if (classId == WrapperTypeInfo::ObjectClassId) {
            type->visitDOMWrapper(m_isolate, toScriptWrappable(wrapper), v8::Persistent<v8::Object>::Cast(*value));
        } else {
            ASSERT_NOT_REACHED();
        }
    }

    void notifyFinished()
    {
        if (!m_constructRetainedObjectInfos)
            return;
        std::sort(m_groupsWhichNeedRetainerInfo.begin(), m_groupsWhichNeedRetainerInfo.end());
        Node* alreadyAdded = 0;
        v8::HeapProfiler* profiler = m_isolate->GetHeapProfiler();
        for (size_t i = 0; i < m_groupsWhichNeedRetainerInfo.size(); ++i) {
            Node* root = m_groupsWhichNeedRetainerInfo[i];
            if (root != alreadyAdded) {
                profiler->SetRetainedObjectInfo(v8::UniqueId(reinterpret_cast<intptr_t>(root)), new RetainedDOMInfo(root));
                alreadyAdded = root;
            }
        }
        if (m_liveRootGroupIdSet)
            profiler->SetRetainedObjectInfo(liveRootId(), new ActiveDOMObjectsInfo(m_domObjectsWithPendingActivity));
    }

private:
    v8::UniqueId liveRootId()
    {
        const v8::Persistent<v8::Value>& liveRoot = V8PerIsolateData::from(m_isolate)->ensureLiveRoot();
        const intptr_t* idPointer = reinterpret_cast<const intptr_t*>(&liveRoot);
        v8::UniqueId id(*idPointer);
        if (!m_liveRootGroupIdSet) {
            m_isolate->SetObjectGroupId(liveRoot, id);
            m_liveRootGroupIdSet = true;
            ++m_domObjectsWithPendingActivity;
        }
        return id;
    }

    v8::Isolate* m_isolate;
    // v8 guarantees that Blink will not regain control while a v8 GC runs
    // (=> no Oilpan GCs will be triggered), hence raw, untraced members
    // can safely be kept here.
    Vector<RawPtrWillBeUntracedMember<Node>> m_groupsWhichNeedRetainerInfo;
    int m_domObjectsWithPendingActivity;
    bool m_liveRootGroupIdSet;
    bool m_constructRetainedObjectInfos;
};

static unsigned long long usedHeapSize(v8::Isolate* isolate)
{
    v8::HeapStatistics heapStatistics;
    isolate->GetHeapStatistics(&heapStatistics);
    return heapStatistics.used_heap_size();
}

namespace {

void visitWeakHandlesForMinorGC(v8::Isolate* isolate)
{
    MinorGCUnmodifiedWrapperVisitor visitor(isolate);
    isolate->VisitWeakHandles(&visitor);
}

void objectGroupingForMajorGC(v8::Isolate* isolate, bool constructRetainedObjectInfos)
{
    MajorGCWrapperVisitor visitor(isolate, constructRetainedObjectInfos);
    isolate->VisitHandlesWithClassIds(&visitor);
    visitor.notifyFinished();
}

void gcPrologueForMajorGC(v8::Isolate* isolate, bool constructRetainedObjectInfos)
{
    if (isMainThread()) {
        {
            TRACE_EVENT_SCOPED_SAMPLING_STATE("blink", "DOMMajorGC");
            objectGroupingForMajorGC(isolate, constructRetainedObjectInfos);
        }
        V8PerIsolateData::from(isolate)->setPreviousSamplingState(TRACE_EVENT_GET_SAMPLING_STATE());
        TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8MajorGC");
    } else {
        objectGroupingForMajorGC(isolate, constructRetainedObjectInfos);
    }
}

}

void V8GCController::gcPrologue(v8::Isolate* isolate, v8::GCType type, v8::GCCallbackFlags flags)
{
    if (isMainThread())
        ScriptForbiddenScope::enter();

    // TODO(haraken): A GC callback is not allowed to re-enter V8. This means
    // that it's unsafe to run Oilpan's GC in the GC callback because it may
    // run finalizers that call into V8. To avoid the risk, we should post
    // a task to schedule the Oilpan's GC.
    // (In practice, there is no finalizer that calls into V8 and thus is safe.)
    if (ThreadState::current())
        ThreadState::current()->willStartV8GC();

    v8::HandleScope scope(isolate);
    switch (type) {
    case v8::kGCTypeScavenge:
        TRACE_EVENT_BEGIN1("devtools.timeline,v8", "MinorGC", "usedHeapSizeBefore", usedHeapSize(isolate));
        if (isMainThread()) {
            TRACE_EVENT_SCOPED_SAMPLING_STATE("blink", "DOMMinorGC");
        }
        visitWeakHandlesForMinorGC(isolate);
        if (isMainThread()) {
            V8PerIsolateData::from(isolate)->setPreviousSamplingState(TRACE_EVENT_GET_SAMPLING_STATE());
            TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8MinorGC");
        }
        break;
    case v8::kGCTypeMarkSweepCompact:
        TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC", "usedHeapSizeBefore", usedHeapSize(isolate), "type", "atomic pause");
        gcPrologueForMajorGC(isolate, flags & v8::kGCCallbackFlagConstructRetainedObjectInfos);
        break;
    case v8::kGCTypeIncrementalMarking:
        TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC", "usedHeapSizeBefore", usedHeapSize(isolate), "type", "incremental marking");
        gcPrologueForMajorGC(isolate, flags & v8::kGCCallbackFlagConstructRetainedObjectInfos);
        break;
    case v8::kGCTypeProcessWeakCallbacks:
        TRACE_EVENT_BEGIN2("devtools.timeline,v8", "MajorGC", "usedHeapSizeBefore", usedHeapSize(isolate), "type", "weak processing");
        if (isMainThread()) {
            V8PerIsolateData::from(isolate)->setPreviousSamplingState(TRACE_EVENT_GET_SAMPLING_STATE());
            TRACE_EVENT_SET_SAMPLING_STATE("blink", "DOMMajorGC");
        }
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void V8GCController::gcEpilogue(v8::Isolate* isolate, v8::GCType type, v8::GCCallbackFlags flags)
{
    switch (type) {
    case v8::kGCTypeScavenge:
        TRACE_EVENT_END1("devtools.timeline,v8", "MinorGC", "usedHeapSizeAfter", usedHeapSize(isolate));
        if (isMainThread()) {
            TRACE_EVENT_SET_NONCONST_SAMPLING_STATE(V8PerIsolateData::from(isolate)->previousSamplingState());
        }
        // TODO(haraken): Remove this. See the comment in gcPrologue.
        if (ThreadState::current())
            ThreadState::current()->scheduleV8FollowupGCIfNeeded(BlinkGC::V8MinorGC);
        break;
    case v8::kGCTypeMarkSweepCompact:
        TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter", usedHeapSize(isolate));
        if (isMainThread()) {
            TRACE_EVENT_SET_NONCONST_SAMPLING_STATE(V8PerIsolateData::from(isolate)->previousSamplingState());
        }
        break;
    case v8::kGCTypeIncrementalMarking:
        TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter", usedHeapSize(isolate));
        if (isMainThread()) {
            TRACE_EVENT_SET_NONCONST_SAMPLING_STATE(V8PerIsolateData::from(isolate)->previousSamplingState());
        }
        break;
    case v8::kGCTypeProcessWeakCallbacks:
        TRACE_EVENT_END1("devtools.timeline,v8", "MajorGC", "usedHeapSizeAfter", usedHeapSize(isolate));
        if (isMainThread()) {
            TRACE_EVENT_SET_NONCONST_SAMPLING_STATE(V8PerIsolateData::from(isolate)->previousSamplingState());
        }
        // TODO(haraken): Remove this. See the comment in gcPrologue.
        if (ThreadState::current())
            ThreadState::current()->scheduleV8FollowupGCIfNeeded(BlinkGC::V8MajorGC);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    if (isMainThread())
        ScriptForbiddenScope::exit();

    // v8::kGCCallbackFlagForced forces a Blink heap garbage collection
    // when a garbage collection was forced from V8. This is either used
    // for tests that force GCs from JavaScript to verify that objects die
    // when expected, or when handling memory pressure notifications.
    if (flags & v8::kGCCallbackFlagForced) {
        // This single GC is not enough for two reasons:
        //   (1) The GC is not precise because the GC scans on-stack pointers conservatively.
        //   (2) One GC is not enough to break a chain of persistent handles. It's possible that
        //       some heap allocated objects own objects that contain persistent handles
        //       pointing to other heap allocated objects. To break the chain, we need multiple GCs.
        //
        // Regarding (1), we force a precise GC at the end of the current event loop. So if you want
        // to collect all garbage, you need to wait until the next event loop.
        // Regarding (2), it would be OK in practice to trigger only one GC per gcEpilogue, because
        // GCController.collectAll() forces 7 V8's GC.
        Heap::collectGarbage(BlinkGC::HeapPointersOnStack, BlinkGC::GCWithSweep, BlinkGC::ForcedGC);

        // Forces a precise GC at the end of the current event loop.
        if (ThreadState::current()) {
            // Temporary asserts to diagnose crbug.com/571207's failure to transition
            // to FullGCScheduled.
            RELEASE_ASSERT(!ThreadState::current()->isSweepingInProgress());
            RELEASE_ASSERT(!ThreadState::current()->isInGC());
            ThreadState::current()->setGCState(ThreadState::FullGCScheduled);
        }
    }

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data", InspectorUpdateCountersEvent::data());
}

void V8GCController::collectGarbage(v8::Isolate* isolate)
{
    v8::HandleScope handleScope(isolate);
    RefPtr<ScriptState> scriptState = ScriptState::create(v8::Context::New(isolate), DOMWrapperWorld::create(isolate));
    ScriptState::Scope scope(scriptState.get());
    V8ScriptRunner::compileAndRunInternalScript(v8String(isolate, "if (gc) gc();"), isolate);
    scriptState->disposePerContextData();
}

void V8GCController::collectAllGarbageForTesting(v8::Isolate* isolate)
{
    for (unsigned i = 0; i < 5; i++)
        isolate->RequestGarbageCollectionForTesting(v8::Isolate::kFullGarbageCollection);
}

void V8GCController::reportDOMMemoryUsageToV8(v8::Isolate* isolate)
{
    // TODO(haraken): Oilpan should report the amount of memory used
    // by DOM nodes as well. Currently Partitions::currentDOMMemoryUsage()
    // just returns 0.
#if !ENABLE(OILPAN)
    if (!isMainThread())
        return;

    static size_t lastUsageReportedToV8 = 0;

    size_t currentUsage = WTF::Partitions::currentDOMMemoryUsage();
    int64_t diff = static_cast<int64_t>(currentUsage) - static_cast<int64_t>(lastUsageReportedToV8);
    isolate->AdjustAmountOfExternalAllocatedMemory(diff);

    lastUsageReportedToV8 = currentUsage;
#endif
}

class DOMWrapperTracer : public v8::PersistentHandleVisitor {
public:
    explicit DOMWrapperTracer(Visitor* visitor)
        : m_visitor(visitor)
    {
    }

    void VisitPersistentHandle(v8::Persistent<v8::Value>* value, uint16_t classId) override
    {
        if (classId != WrapperTypeInfo::NodeClassId && classId != WrapperTypeInfo::ObjectClassId)
            return;

        const v8::Persistent<v8::Object>& wrapper = v8::Persistent<v8::Object>::Cast(*value);

        if (m_visitor)
            toWrapperTypeInfo(wrapper)->trace(m_visitor, toScriptWrappable(wrapper));
    }

private:
    Visitor* m_visitor;
};

void V8GCController::traceDOMWrappers(v8::Isolate* isolate, Visitor* visitor)
{
    DOMWrapperTracer tracer(visitor);
    isolate->VisitHandlesWithClassIds(&tracer);
}

} // namespace blink
