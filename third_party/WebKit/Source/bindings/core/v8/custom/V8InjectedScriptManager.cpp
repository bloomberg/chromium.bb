/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "core/inspector/InjectedScriptManager.h"

#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/inspector/InjectedScriptHost.h"
#include "core/inspector/InjectedScriptNative.h"
#include "core/inspector/v8/V8InjectedScriptHost.h"
#include "wtf/RefPtr.h"

namespace blink {

v8::Local<v8::Object> InjectedScriptManager::createInjectedScript(const String& scriptSource, v8::Local<v8::Context> context, int id, InjectedScriptNative* injectedScriptNative)
{
    v8::Isolate* isolate = context->GetIsolate();
    v8::Context::Scope scope(context);

    v8::Local<v8::FunctionTemplate> wrapperTemplate = m_injectedScriptHost->wrapperTemplate(isolate);
    if (wrapperTemplate.IsEmpty()) {
        wrapperTemplate = V8InjectedScriptHost::createWrapperTemplate(isolate);
        m_injectedScriptHost->setWrapperTemplate(wrapperTemplate, isolate);
    }

    v8::Local<v8::Object> scriptHostWrapper = V8InjectedScriptHost::wrap(wrapperTemplate, context, m_injectedScriptHost);
    if (scriptHostWrapper.IsEmpty())
        return v8::Local<v8::Object>();

    injectedScriptNative->setOnInjectedScriptHost(scriptHostWrapper);

    // Inject javascript into the context. The compiled script is supposed to evaluate into
    // a single anonymous function(it's anonymous to avoid cluttering the global object with
    // inspector's stuff) the function is called a few lines below with InjectedScriptHost wrapper,
    // injected script id and explicit reference to the inspected global object. The function is expected
    // to create and configure InjectedScript instance that is going to be used by the inspector.
    v8::Local<v8::Value> value;
    if (!V8ScriptRunner::compileAndRunInternalScript(v8String(isolate, scriptSource), isolate).ToLocal(&value))
        return v8::Local<v8::Object>();
    ASSERT(value->IsFunction());

    v8::Local<v8::Object> windowGlobal = context->Global();
    v8::Local<v8::Value> info[] = { scriptHostWrapper, windowGlobal, v8::Number::New(context->GetIsolate(), id) };
    v8::Local<v8::Value> injectedScriptValue;
    if (!V8ScriptRunner::callInternalFunction(v8::Local<v8::Function>::Cast(value), windowGlobal, WTF_ARRAY_LENGTH(info), info, context->GetIsolate()).ToLocal(&injectedScriptValue))
        return v8::Local<v8::Object>();
    if (!injectedScriptValue->IsObject())
        return v8::Local<v8::Object>();
    return injectedScriptValue.As<v8::Object>();
}

} // namespace blink
