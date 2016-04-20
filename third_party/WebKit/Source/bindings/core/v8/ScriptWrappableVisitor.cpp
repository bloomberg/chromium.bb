// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptWrappableVisitor.h"

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/DOMDataStore.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/WrapperTypeInfo.h"
#include "core/dom/NodeRareData.h"
#include "core/events/EventListenerMap.h"
#include "core/events/EventTarget.h"
#include "platform/heap/HeapPage.h"

namespace blink {

ScriptWrappableVisitor::~ScriptWrappableVisitor()
{
}

void ScriptWrappableVisitor::TracePrologue()
{
    m_tracingInProgress = true;
    ActiveScriptWrappable::traceActiveScriptWrappables(this);
}

void ScriptWrappableVisitor::TraceEpilogue()
{
    for (auto header : m_headersToUnmark)
        header->unmarkWrapperHeader();

    m_headersToUnmark.clear();
    m_tracingInProgress = false;
}

void ScriptWrappableVisitor::TraceWrappersFrom(const std::vector<std::pair<void*, void*>>& internalFieldsOfPotentialWrappers)
{
    ASSERT(m_tracingInProgress);
    for (auto pair : internalFieldsOfPotentialWrappers) {
        traceWrappersFrom(pair);
    }
}

void ScriptWrappableVisitor::traceWrappersFrom(std::pair<void*, void*> internalFields)
{
    if (reinterpret_cast<WrapperTypeInfo*>(internalFields.first)->ginEmbedder != gin::GinEmbedder::kEmbedderBlink)
        return;

    ScriptWrappable* scriptWrappable = reinterpret_cast<ScriptWrappable*>(internalFields.second);
    ASSERT(scriptWrappable->wrapperTypeInfo()->wrapperClassId == WrapperTypeInfo::NodeClassId
        || scriptWrappable->wrapperTypeInfo()->wrapperClassId == WrapperTypeInfo::ObjectClassId);

    traceWrappers(scriptWrappable);
}

bool ScriptWrappableVisitor::isHeaderMarked(const void* garbageCollected) const
{
    return HeapObjectHeader::fromPayload(garbageCollected)->isWrapperHeaderMarked();
}

void ScriptWrappableVisitor::markHeader(const void* garbageCollected) const
{
    HeapObjectHeader* header = HeapObjectHeader::fromPayload(garbageCollected);
    header->markWrapperHeader();
    addHeaderToUnmark(header);
}

void ScriptWrappableVisitor::markHeader(const ScriptWrappable* scriptWrappable) const
{
    markHeader(static_cast<const void*>(scriptWrappable));

    markWrappersInAllWorlds(scriptWrappable, m_isolate);
}

void ScriptWrappableVisitor::addHeaderToUnmark(HeapObjectHeader* header) const
{
    m_headersToUnmark.append(header);
}

void ScriptWrappableVisitor::markWrapper(const v8::Persistent<v8::Object>& handle, v8::Isolate* isolate)
{
    handle.RegisterExternalReference(isolate);
}

void ScriptWrappableVisitor::markWrappersInAllWorlds(const ScriptWrappable* scriptWrappable, v8::Isolate* isolate)
{
    DOMWrapperWorld::markWrappersInAllWorlds(const_cast<ScriptWrappable*>(scriptWrappable), isolate);
}

void ScriptWrappableVisitor::traceWrappers(const ScriptWrappable* wrappable) const
{
    if (wrappable && !isHeaderMarked(wrappable)) {
        markHeader(wrappable);
        wrappable->traceWrappers(this);
    }
}

void ScriptWrappableVisitor::traceWrappers(const ScriptWrappable& wrappable) const
{
    traceWrappers(&wrappable);
}

} // namespace blink
