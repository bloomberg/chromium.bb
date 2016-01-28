// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/ThreadDebugger.h"

#include "bindings/core/v8/V8Binding.h"
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

void ThreadDebugger::eventListeners(v8::Local<v8::Value> value, EventListenerInfoMap& result)
{
    InspectorDOMDebuggerAgent::eventListenersInfoForTarget(m_isolate, value, result);
}

v8::MaybeLocal<v8::Value> ThreadDebugger::compileAndRunInternalScript(const String& script)
{
    return V8ScriptRunner::compileAndRunInternalScript(v8String(m_isolate, script), m_isolate);
}

v8::MaybeLocal<v8::Value> ThreadDebugger::callFunction(v8::Local<v8::Function> function, v8::Local<v8::Context> context, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> info[])
{
    return V8ScriptRunner::callFunction(function, toExecutionContext(context), receiver, argc, info, m_isolate);
}

v8::MaybeLocal<v8::Value> ThreadDebugger::callInternalFunction(v8::Local<v8::Function> function, v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> info[])
{
    return V8ScriptRunner::callInternalFunction(function, receiver, argc, info, m_isolate);
}

} // namespace blink
