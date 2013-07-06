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
#include "bindings/v8/CustomElementHelpers.h"
#include "bindings/v8/DOMDataStore.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "core/dom/ScriptExecutionContext.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

PassRefPtr<V8CustomElementLifecycleCallbacks> V8CustomElementLifecycleCallbacks::create(ScriptExecutionContext* scriptExecutionContext, v8::Handle<v8::Object> prototype, v8::Handle<v8::Function> created, v8::Handle<v8::Function> attributeChanged)
{
    // A given object can only be used as a Custom Element prototype once, see isCustomElementInterfacePrototypeObject
    ASSERT(prototype->GetHiddenValue(V8HiddenPropertyName::customElementCreated()).IsEmpty() && prototype->GetHiddenValue(V8HiddenPropertyName::customElementAttributeChanged()).IsEmpty());
    if (!created.IsEmpty())
        prototype->SetHiddenValue(V8HiddenPropertyName::customElementCreated(), created);
    if (!attributeChanged.IsEmpty())
        prototype->SetHiddenValue(V8HiddenPropertyName::customElementAttributeChanged(), attributeChanged);
    return adoptRef(new V8CustomElementLifecycleCallbacks(scriptExecutionContext, prototype, created, attributeChanged));
}

template <typename T>
static void weakCallback(v8::Isolate*, v8::Persistent<T>*, ScopedPersistent<T>* handle)
{
    handle->clear();
}

V8CustomElementLifecycleCallbacks::V8CustomElementLifecycleCallbacks(ScriptExecutionContext* scriptExecutionContext, v8::Handle<v8::Object> prototype, v8::Handle<v8::Function> created, v8::Handle<v8::Function> attributeChanged)
    : CustomElementLifecycleCallbacks(CallbackType(Created | (attributeChanged.IsEmpty() ? None : AttributeChanged)))
    , ActiveDOMCallback(scriptExecutionContext)
    , m_world(DOMWrapperWorld::current())
    , m_prototype(prototype)
    , m_created(created)
    , m_attributeChanged(attributeChanged)
{
    m_prototype.makeWeak(&m_prototype, weakCallback<v8::Object>);
    if (!m_created.isEmpty())
        m_created.makeWeak(&m_created, weakCallback<v8::Function>);
    if (!m_attributeChanged.isEmpty())
        m_attributeChanged.makeWeak(&m_attributeChanged, weakCallback<v8::Function>);
}

void V8CustomElementLifecycleCallbacks::created(Element* element)
{
    if (!canInvokeCallback())
        return;

    element->setIsUpgradedCustomElement();

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = toV8Context(scriptExecutionContext(), m_world.get());
    if (context.IsEmpty())
        return;

    v8::Context::Scope scope(context);
    v8::Isolate* isolate = context->GetIsolate();

    v8::Handle<v8::Object> receiver = DOMDataStore::current(isolate)->get(element);
    if (!receiver.IsEmpty()) {
        // Swizzle the prototype of the existing wrapper. We don't need to
        // worry about non-existent wrappers; they will get the right
        // prototype when wrapped.
        v8::Handle<v8::Object> prototype = m_prototype.newLocal(isolate);
        if (prototype.IsEmpty())
            return;
        receiver->SetPrototype(prototype);
    }

    v8::Handle<v8::Function> callback = m_created.newLocal(isolate);
    if (callback.IsEmpty())
        return;

    if (receiver.IsEmpty())
        receiver = toV8(element, context->Global(), isolate).As<v8::Object>();

    ASSERT(!receiver.IsEmpty());

    v8::TryCatch exceptionCatcher;
    exceptionCatcher.SetVerbose(true);
    ScriptController::callFunctionWithInstrumentation(scriptExecutionContext(), callback, receiver, 0, 0);
}

void V8CustomElementLifecycleCallbacks::attributeChanged(Element* element, const AtomicString& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (!canInvokeCallback())
        return;

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = toV8Context(scriptExecutionContext(), m_world.get());
    if (context.IsEmpty())
        return;

    v8::Context::Scope scope(context);
    v8::Isolate* isolate = context->GetIsolate();

    v8::Handle<v8::Object> receiver = toV8(element, context->Global(), isolate).As<v8::Object>();
    ASSERT(!receiver.IsEmpty());

    v8::Handle<v8::Function> callback = m_attributeChanged.newLocal(isolate);
    if (callback.IsEmpty())
        return;

    v8::Handle<v8::Value> argv[] = {
        v8String(name, isolate),
        oldValue.isNull() ? v8::Handle<v8::Value>(v8::Null()) : v8::Handle<v8::Value>(v8String(oldValue, isolate)),
        newValue.isNull() ? v8::Handle<v8::Value>(v8::Null()) : v8::Handle<v8::Value>(v8String(newValue, isolate))
    };

    v8::TryCatch exceptionCatcher;
    exceptionCatcher.SetVerbose(true);
    ScriptController::callFunctionWithInstrumentation(scriptExecutionContext(), callback, receiver, sizeof(argv) / sizeof(v8::Handle<v8::Value>), argv);
}

} // namespace WebCore
