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
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/inspector/InspectorDOMDebuggerAgent.h"

namespace blink {

ThreadDebugger::ThreadDebugger(v8::Isolate* isolate)
    : m_isolate(isolate)
    , m_debugger(V8Debugger::create(isolate, this))
{
}

ThreadDebugger::~ThreadDebugger()
{
}

void ThreadDebugger::eventListeners(v8::Local<v8::Value> value, V8EventListenerInfoMap& result)
{
    InspectorDOMDebuggerAgent::eventListenersInfoForTarget(m_isolate, value, result);
}

v8::MaybeLocal<v8::Object> ThreadDebugger::instantiateObject(v8::Local<v8::Function> function)
{
    return V8ScriptRunner::instantiateObject(m_isolate, function);
}

v8::MaybeLocal<v8::Value> ThreadDebugger::runCompiledScript(v8::Local<v8::Context> context, v8::Local<v8::Script> script)
{
    return V8ScriptRunner::runCompiledScript(m_isolate, script, toExecutionContext(context));
}

v8::MaybeLocal<v8::Value> ThreadDebugger::compileAndRunInternalScript(v8::Local<v8::String> script)
{
    return V8ScriptRunner::compileAndRunInternalScript(script, m_isolate);
}

v8::MaybeLocal<v8::Value> ThreadDebugger::callFunction(v8::Local<v8::Function> function, v8::Local<v8::Context> context, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> info[])
{
    return V8ScriptRunner::callFunction(function, toExecutionContext(context), receiver, argc, info, m_isolate);
}

v8::MaybeLocal<v8::Value> ThreadDebugger::callInternalFunction(v8::Local<v8::Function> function, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> info[])
{
    return V8ScriptRunner::callInternalFunction(function, receiver, argc, info, m_isolate);
}

String ThreadDebugger::valueSubtype(v8::Local<v8::Value> value)
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

} // namespace blink
