// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptWrappableVisitor.h"

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/WrapperTypeInfo.h"
#include "core/html/imports/HTMLImportsController.h"
#include "platform/heap/HeapPage.h"

namespace blink {

ScriptWrappableVisitor::~ScriptWrappableVisitor()
{
}

void ScriptWrappableVisitor::TracePrologue()
{
    m_tracingInProgress = true;
    ActiveScriptWrappable::traceActiveScriptWrappables(m_isolate, this);
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
    // TODO(hlopko): Visit the vector in the V8 instead of passing it over if
    // there is no performance impact
    for (auto pair : internalFieldsOfPotentialWrappers) {
        traceWrappersFrom(pair);
    }
}

void ScriptWrappableVisitor::traceWrappersFrom(std::pair<void*, void*> internalFields)
{
    WrapperTypeInfo* wrapperTypeInfo = reinterpret_cast<WrapperTypeInfo*>(internalFields.first);
    if (wrapperTypeInfo->ginEmbedder != gin::GinEmbedder::kEmbedderBlink)
        return;

    ScriptWrappable* scriptWrappable = reinterpret_cast<ScriptWrappable*>(internalFields.second);
    ASSERT(wrapperTypeInfo->wrapperClassId == WrapperTypeInfo::NodeClassId
        || wrapperTypeInfo->wrapperClassId == WrapperTypeInfo::ObjectClassId);

    wrapperTypeInfo->traceWrappers(this, scriptWrappable);
}

bool ScriptWrappableVisitor::markWrapperHeader(const void* garbageCollected, const void* objectTopPointer) const
{
    HeapObjectHeader* header = HeapObjectHeader::fromPayload(objectTopPointer);
    if (header->isWrapperHeaderMarked())
        return false;

    header->markWrapperHeader();
    addHeaderToUnmark(header);
    return true;
}

bool ScriptWrappableVisitor::markWrapperHeader(const ScriptWrappable* scriptWrappable, const void* objectTopPointer) const
{
    if (!markWrapperHeader(objectTopPointer, objectTopPointer))
        return false;

    markWrappersInAllWorlds(scriptWrappable, m_isolate);
    return true;
}

void ScriptWrappableVisitor::addHeaderToUnmark(HeapObjectHeader* header) const
{
    m_headersToUnmark.append(header);
}

void ScriptWrappableVisitor::markWrapper(const v8::Persistent<v8::Object>* handle, v8::Isolate* isolate)
{
    handle->RegisterExternalReference(isolate);
}

void ScriptWrappableVisitor::markWrapper(const v8::Persistent<v8::Object>* handle) const
{
    markWrapper(handle, m_isolate);
}

void ScriptWrappableVisitor::markWrappersInAllWorlds(const ScriptWrappable* scriptWrappable, v8::Isolate* isolate)
{
    DOMWrapperWorld::markWrappersInAllWorlds(const_cast<ScriptWrappable*>(scriptWrappable), isolate);
}

void ScriptWrappableVisitor::dispatchTraceWrappers(const ScriptWrappable* wrappable) const
{
    wrappable->traceWrappers(this);
}

#define DEFINE_DISPATCH_TRACE_WRAPPERS(className)                 \
void ScriptWrappableVisitor::dispatchTraceWrappers(const className* traceable) const \
{                                                                 \
    traceable->traceWrappers(this);                               \
}                                                                 \

WRAPPER_VISITOR_SPECIAL_CLASSES(DEFINE_DISPATCH_TRACE_WRAPPERS);

#undef DEFINE_DISPATCH_TRACE_WRAPPERS

} // namespace blink
