// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptWrappableVisitor.h"

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/WrapperTypeInfo.h"
#include "core/dom/DocumentStyleSheetCollection.h"
#include "core/dom/ElementRareData.h"
#include "core/dom/NodeListsNodeData.h"
#include "core/dom/NodeRareData.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/html/imports/HTMLImportsController.h"
#include "platform/heap/HeapPage.h"

namespace blink {

ScriptWrappableVisitor::~ScriptWrappableVisitor()
{
}

void ScriptWrappableVisitor::TracePrologue()
{
    m_tracingInProgress = true;
}

void ScriptWrappableVisitor::TraceEpilogue()
{
    ActiveScriptWrappable::traceActiveScriptWrappables(m_isolate, this);
    AdvanceTracing(0, AdvanceTracingActions(ForceCompletionAction::FORCE_COMPLETION));

    for (auto header : m_headersToUnmark) {
        header->unmarkWrapperHeader();
    }

    m_headersToUnmark.clear();
    m_tracingInProgress = false;
}

void ScriptWrappableVisitor::RegisterV8Reference(const std::pair<void*, void*>& internalFields)
{
    if (!m_tracingInProgress) {
        return;
    }

    WrapperTypeInfo* wrapperTypeInfo = reinterpret_cast<WrapperTypeInfo*>(internalFields.first);
    if (wrapperTypeInfo->ginEmbedder != gin::GinEmbedder::kEmbedderBlink) {
        return;
    }
    DCHECK(wrapperTypeInfo->wrapperClassId == WrapperTypeInfo::NodeClassId
        || wrapperTypeInfo->wrapperClassId == WrapperTypeInfo::ObjectClassId);

    ScriptWrappable* scriptWrappable = reinterpret_cast<ScriptWrappable*>(internalFields.second);
    if (wrapperTypeInfo->getHeapObjectHeader(scriptWrappable)->isWrapperHeaderMarked()) {
        return;
    }

    m_markingDeque.append(scriptWrappable);
}

void ScriptWrappableVisitor::RegisterV8References(const std::vector<std::pair<void*, void*>>& internalFieldsOfPotentialWrappers)
{
    // TODO(hlopko): Visit the vector in the V8 instead of passing it over if
    // there is no performance impact
    for (auto& pair : internalFieldsOfPotentialWrappers) {
        RegisterV8Reference(pair);
    }
}

bool ScriptWrappableVisitor::AdvanceTracing(double deadlineInMs, v8::EmbedderHeapTracer::AdvanceTracingActions actions)
{
    DCHECK(m_tracingInProgress);
    while (actions.force_completion == v8::EmbedderHeapTracer::ForceCompletionAction::FORCE_COMPLETION
        || WTF::monotonicallyIncreasingTimeMS() < deadlineInMs) {
        if (m_markingDeque.isEmpty()) {
            return false;
        }

        const ScriptWrappable* scriptWrappable = m_markingDeque.takeFirst();
        // there might be nullptrs in the deque after oilpan gcs
        if (scriptWrappable) {
            markWrapperHeader(scriptWrappable);
            scriptWrappable->traceWrappers(this);
        }
    }
    return true;
}

bool ScriptWrappableVisitor::markWrapperHeader(HeapObjectHeader* header) const
{
    if (header->isWrapperHeaderMarked())
        return false;

    header->markWrapperHeader();
    m_headersToUnmark.append(header);
    return true;
}

bool ScriptWrappableVisitor::markWrapperHeader(const void* garbageCollected) const
{
    HeapObjectHeader* header = HeapObjectHeader::fromPayload(garbageCollected);
    return markWrapperHeader(header);
}

bool ScriptWrappableVisitor::markWrapperHeader(const ScriptWrappable* scriptWrappable) const
{
    if (!markWrapperHeader(scriptWrappable->wrapperTypeInfo()->
        getHeapObjectHeader(const_cast<ScriptWrappable*>(scriptWrappable)))) {
        return false;
    }

    markWrappersInAllWorlds(scriptWrappable, m_isolate);
    return true;
}

void ScriptWrappableVisitor::markWrappersInAllWorlds(const ScriptWrappable* scriptWrappable, v8::Isolate* isolate)
{
    DOMWrapperWorld::markWrappersInAllWorlds(const_cast<ScriptWrappable*>(scriptWrappable), isolate);
}

void ScriptWrappableVisitor::traceWrappers(const ScopedPersistent<v8::Value>* scopedPersistent) const
{
    markWrapper(
        &(const_cast<ScopedPersistent<v8::Value>*>(scopedPersistent)->get()));
}

void ScriptWrappableVisitor::dispatchTraceWrappers(const ScriptWrappable* wrappable) const
{
    wrappable->traceWrappers(this);
}

#define DEFINE_DISPATCH_TRACE_WRAPPERS(className)                    \
void ScriptWrappableVisitor::dispatchTraceWrappers(const className* traceable) const \
{                                                                    \
    traceable->traceWrappers(this);                                  \
}                                                                    \

WRAPPER_VISITOR_SPECIAL_CLASSES(DEFINE_DISPATCH_TRACE_WRAPPERS);

#undef DEFINE_DISPATCH_TRACE_WRAPPERS

void ScriptWrappableVisitor::invalidateDeadObjectsInMarkingDeque()
{
    for (auto it = m_markingDeque.begin(); it != m_markingDeque.end(); ++it) {
        const ScriptWrappable* scriptWrappable = *it;
        if (!scriptWrappable->wrapperTypeInfo()->
            getHeapObjectHeader(const_cast<ScriptWrappable*>(scriptWrappable))->
            isMarked()) {
            *it = nullptr;
        }
    }
}

void ScriptWrappableVisitor::invalidateDeadObjectsInMarkingDeque(v8::Isolate* isolate)
{
    ScriptWrappableVisitor* scriptWrappableVisitor = V8PerIsolateData::from(isolate)->scriptWrappableVisitor();
    if (scriptWrappableVisitor) {
        scriptWrappableVisitor->invalidateDeadObjectsInMarkingDeque();
    }
}

} // namespace blink
