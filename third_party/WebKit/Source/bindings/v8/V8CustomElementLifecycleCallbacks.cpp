/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "bindings/v8/V8CustomElementLifecycleCallbacks.h"

#include "V8Element.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "core/dom/ScriptExecutionContext.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

PassRefPtr<V8CustomElementLifecycleCallbacks> V8CustomElementLifecycleCallbacks::create(ScriptExecutionContext* scriptExecutionContext, v8::Handle<v8::Object> owner, v8::Handle<v8::Function> created)
{
    if (!created.IsEmpty())
        owner->SetHiddenValue(V8HiddenPropertyName::customElementCreated(), created);
    return adoptRef(new V8CustomElementLifecycleCallbacks(scriptExecutionContext, created));
}

static void weakCallback(v8::Isolate*, v8::Persistent<v8::Function>*, ScopedPersistent<v8::Function>* handle)
{
    handle->clear();
}

V8CustomElementLifecycleCallbacks::V8CustomElementLifecycleCallbacks(ScriptExecutionContext* scriptExecutionContext, v8::Handle<v8::Function> created)
    : CustomElementLifecycleCallbacks(created.IsEmpty() ? None : Created)
    , ActiveDOMCallback(scriptExecutionContext)
    , m_world(DOMWrapperWorld::current())
    , m_created(created)
{
    if (!m_created.isEmpty())
        m_created.makeWeak(&m_created, weakCallback);
}

void V8CustomElementLifecycleCallbacks::created(Element* element)
{
    if (!canInvokeCallback())
        return;

    v8::HandleScope handleScope;

    v8::Handle<v8::Context> context = toV8Context(scriptExecutionContext(), m_world.get());
    if (context.IsEmpty())
        return;

    v8::Context::Scope scope(context);
    v8::Isolate* isolate = context->GetIsolate();

    v8::Handle<v8::Function> callback = m_created.newLocal(isolate);
    if (callback.IsEmpty())
        return;

    v8::Handle<v8::Value> elementHandle = toV8(element, context->Global(), isolate);
    if (elementHandle.IsEmpty()) {
        if (!isScriptControllerTerminating())
            CRASH();
        return;
    }

    ASSERT(elementHandle->IsObject());
    v8::Handle<v8::Object> receiver = v8::Handle<v8::Object>::Cast(elementHandle);

    v8::TryCatch exceptionCatcher;
    exceptionCatcher.SetVerbose(true);
    ScriptController::callFunctionWithInstrumentation(scriptExecutionContext(), callback, receiver, 0, 0);
}

} // namespace WebCore

