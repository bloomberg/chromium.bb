// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/ThreadDebugger.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8DOMException.h"
#include "bindings/core/v8/V8DOMTokenList.h"
#include "bindings/core/v8/V8HTMLAllCollection.h"
#include "bindings/core/v8/V8HTMLCollection.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8NodeList.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"
#include "platform/ScriptForbiddenScope.h"
#include "wtf/CurrentTime.h"

namespace blink {

ThreadDebugger::ThreadDebugger(v8::Isolate* isolate)
    : m_isolate(isolate)
    , m_debugger(V8Debugger::create(isolate, this))
{
}

ThreadDebugger::~ThreadDebugger()
{
}

void ThreadDebugger::eventListeners(v8::Local<v8::Value> value, V8EventListenerInfoList& result)
{
    InspectorDOMDebuggerAgent::eventListenersInfoForTarget(m_isolate, value, result);
}

String16 ThreadDebugger::valueSubtype(v8::Local<v8::Value> value)
{
    if (V8Node::hasInstance(value, m_isolate))
        return "node";
    if (V8NodeList::hasInstance(value, m_isolate)
        || V8DOMTokenList::hasInstance(value, m_isolate)
        || V8HTMLCollection::hasInstance(value, m_isolate)
        || V8HTMLAllCollection::hasInstance(value, m_isolate)) {
        return "array";
    }
    if (V8DOMException::hasInstance(value, m_isolate))
        return "error";
    return String();
}

bool ThreadDebugger::formatAccessorsAsProperties(v8::Local<v8::Value> value)
{
    return V8DOMWrapper::isWrapper(m_isolate, value);
}

bool ThreadDebugger::isExecutionAllowed()
{
    return !ScriptForbiddenScope::isScriptForbidden();
}

double ThreadDebugger::currentTimeMS()
{
    return WTF::currentTimeMS();
}


} // namespace blink
