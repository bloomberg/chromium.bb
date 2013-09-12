/*
 * Copyright (C) 2009, 2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "bindings/v8/ScriptState.h"

#include "V8Window.h"
#include "V8WorkerGlobalScope.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "bindings/v8/WorkerScriptController.h"
#include "core/page/Frame.h"
#include "core/workers/WorkerGlobalScope.h"
#include <v8.h>
#include "wtf/Assertions.h"

namespace WebCore {

ScriptState::ScriptState(v8::Handle<v8::Context> context)
    : m_context(context)
    , m_isolate(context->GetIsolate())
{
    m_context.makeWeak(this, &makeWeakCallback);
}

ScriptState::~ScriptState()
{
}

DOMWindow* ScriptState::domWindow() const
{
    v8::HandleScope handleScope(m_isolate);
    return toDOMWindow(m_context.newLocal(m_isolate));
}

ScriptExecutionContext* ScriptState::scriptExecutionContext() const
{
    v8::HandleScope handleScope(m_isolate);
    return toScriptExecutionContext(m_context.newLocal(m_isolate));
}

ScriptState* ScriptState::forContext(v8::Handle<v8::Context> context)
{
    v8::Context::Scope contextScope(context);

    v8::Local<v8::Object> innerGlobal = v8::Local<v8::Object>::Cast(context->Global()->GetPrototype());

    v8::Local<v8::Value> scriptStateWrapper = innerGlobal->GetHiddenValue(V8HiddenPropertyName::scriptState());
    if (!scriptStateWrapper.IsEmpty() && scriptStateWrapper->IsExternal())
        return static_cast<ScriptState*>(v8::External::Cast(*scriptStateWrapper)->Value());

    ScriptState* scriptState = new ScriptState(context);
    innerGlobal->SetHiddenValue(V8HiddenPropertyName::scriptState(), v8::External::New(scriptState));
    return scriptState;
}

ScriptState* ScriptState::current()
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    ASSERT(!context.IsEmpty());
    return ScriptState::forContext(context);
}

void ScriptState::makeWeakCallback(v8::Isolate* isolate, v8::Persistent<v8::Context>* object, ScriptState* scriptState)
{
    delete scriptState;
}

bool ScriptState::evalEnabled() const
{
    v8::HandleScope handleScope(m_isolate);
    return context()->IsCodeGenerationFromStringsAllowed();
}

void ScriptState::setEvalEnabled(bool enabled)
{
    v8::HandleScope handleScope(m_isolate);
    return context()->AllowCodeGenerationFromStrings(enabled);
}

ScriptState* mainWorldScriptState(Frame* frame)
{
    v8::HandleScope handleScope(isolateForFrame(frame));
    return ScriptState::forContext(frame->script()->mainWorldContext());
}

ScriptState* scriptStateFromWorkerGlobalScope(WorkerGlobalScope* workerGlobalScope)
{
    WorkerScriptController* script = workerGlobalScope->script();
    if (!script)
        return 0;

    v8::HandleScope handleScope(script->isolate());
    return ScriptState::forContext(script->context());
}

}
