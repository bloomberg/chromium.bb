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
#include "bindings/v8/CustomElementBinding.h"
#include "bindings/v8/DOMDataStore.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "bindings/v8/V8PerContextData.h"
#include "core/dom/ScriptExecutionContext.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

#define CALLBACK_LIST(V)                  \
    V(created, Created)                   \
    V(enteredView, EnteredView)           \
    V(leftView, LeftView)                 \
    V(attributeChanged, AttributeChanged)

PassRefPtr<V8CustomElementLifecycleCallbacks> V8CustomElementLifecycleCallbacks::create(ScriptExecutionContext* scriptExecutionContext, v8::Handle<v8::Object> prototype, v8::Handle<v8::Function> created, v8::Handle<v8::Function> enteredView, v8::Handle<v8::Function> leftView, v8::Handle<v8::Function> attributeChanged)
{
    // A given object can only be used as a Custom Element prototype
    // once; see customElementIsInterfacePrototypeObject
#define SET_HIDDEN_PROPERTY(Value, Name) \
    ASSERT(prototype->GetHiddenValue(V8HiddenPropertyName::customElement##Name()).IsEmpty()); \
    if (!Value.IsEmpty()) \
        prototype->SetHiddenValue(V8HiddenPropertyName::customElement##Name(), Value);

    CALLBACK_LIST(SET_HIDDEN_PROPERTY)
#undef SET_HIDDEN_PROPERTY

    return adoptRef(new V8CustomElementLifecycleCallbacks(scriptExecutionContext, prototype, created, enteredView, leftView, attributeChanged));
}

static CustomElementLifecycleCallbacks::CallbackType flagSet(v8::Handle<v8::Function> enteredView, v8::Handle<v8::Function> leftView, v8::Handle<v8::Function> attributeChanged)
{
    // V8 Custom Elements always run created to swizzle prototypes.
    int flags = CustomElementLifecycleCallbacks::Created;

    if (!enteredView.IsEmpty())
        flags |= CustomElementLifecycleCallbacks::EnteredView;

    if (!leftView.IsEmpty())
        flags |= CustomElementLifecycleCallbacks::LeftView;

    if (!attributeChanged.IsEmpty())
        flags |= CustomElementLifecycleCallbacks::AttributeChanged;

    return CustomElementLifecycleCallbacks::CallbackType(flags);
}

template <typename T>
static void weakCallback(v8::Isolate*, v8::Persistent<T>*, ScopedPersistent<T>* handle)
{
    handle->clear();
}

V8CustomElementLifecycleCallbacks::V8CustomElementLifecycleCallbacks(ScriptExecutionContext* scriptExecutionContext, v8::Handle<v8::Object> prototype, v8::Handle<v8::Function> created, v8::Handle<v8::Function> enteredView, v8::Handle<v8::Function> leftView, v8::Handle<v8::Function> attributeChanged)
    : CustomElementLifecycleCallbacks(flagSet(enteredView, leftView, attributeChanged))
    , ActiveDOMCallback(scriptExecutionContext)
    , m_world(DOMWrapperWorld::current())
    , m_prototype(prototype)
    , m_created(created)
    , m_enteredView(enteredView)
    , m_leftView(leftView)
    , m_attributeChanged(attributeChanged)
    , m_owner(0)
{
    m_prototype.makeWeak(&m_prototype, weakCallback<v8::Object>);

#define MAKE_WEAK(Var, _) \
    if (!m_##Var.isEmpty()) \
        m_##Var.makeWeak(&m_##Var, weakCallback<v8::Function>);

    CALLBACK_LIST(MAKE_WEAK)
#undef MAKE_WEAK
}

V8PerContextData* V8CustomElementLifecycleCallbacks::creationContextData()
{
    if (!scriptExecutionContext())
        return 0;

    v8::Handle<v8::Context> context = toV8Context(scriptExecutionContext(), m_world.get());
    if (context.IsEmpty())
        return 0;

    return V8PerContextData::from(context);
}

V8CustomElementLifecycleCallbacks::~V8CustomElementLifecycleCallbacks()
{
    if (!m_owner)
        return;

    v8::HandleScope handleScope(isolateForScriptExecutionContext(scriptExecutionContext()));
    if (V8PerContextData* perContextData = creationContextData())
        perContextData->clearCustomElementBinding(m_owner);
}

bool V8CustomElementLifecycleCallbacks::setBinding(CustomElementDefinition* owner, PassOwnPtr<CustomElementBinding> binding)
{
    ASSERT(!m_owner);

    V8PerContextData* perContextData = creationContextData();
    if (!perContextData)
        return false;

    m_owner = owner;

    // Bindings retrieve the prototype when needed from per-context data.
    perContextData->addCustomElementBinding(owner, binding);

    return true;
}

void V8CustomElementLifecycleCallbacks::created(Element* element)
{
    if (!canInvokeCallback())
        return;

    element->setCustomElementState(Element::Upgraded);

    v8::Isolate* isolate = isolateForScriptExecutionContext(scriptExecutionContext());
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Context> context = toV8Context(scriptExecutionContext(), m_world.get());
    if (context.IsEmpty())
        return;

    v8::Context::Scope scope(context);

    v8::Handle<v8::Object> receiver = DOMDataStore::current(isolate)->get<V8Element>(element, isolate);
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
    ScriptController::callFunctionWithInstrumentation(scriptExecutionContext(), callback, receiver, 0, 0, isolate);
}

void V8CustomElementLifecycleCallbacks::enteredView(Element* element)
{
    call(m_enteredView, element);
}

void V8CustomElementLifecycleCallbacks::leftView(Element* element)
{
    call(m_leftView, element);
}

void V8CustomElementLifecycleCallbacks::attributeChanged(Element* element, const AtomicString& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (!canInvokeCallback())
        return;

    v8::Isolate* isolate = isolateForScriptExecutionContext(scriptExecutionContext());
    v8::HandleScope handleScope(isolate);
    v8::Handle<v8::Context> context = toV8Context(scriptExecutionContext(), m_world.get());
    if (context.IsEmpty())
        return;

    v8::Context::Scope scope(context);

    v8::Handle<v8::Object> receiver = toV8(element, context->Global(), isolate).As<v8::Object>();
    ASSERT(!receiver.IsEmpty());

    v8::Handle<v8::Function> callback = m_attributeChanged.newLocal(isolate);
    if (callback.IsEmpty())
        return;

    v8::Handle<v8::Value> argv[] = {
        v8String(name, isolate),
        oldValue.isNull() ? v8::Handle<v8::Value>(v8::Null(isolate)) : v8::Handle<v8::Value>(v8String(oldValue, isolate)),
        newValue.isNull() ? v8::Handle<v8::Value>(v8::Null(isolate)) : v8::Handle<v8::Value>(v8String(newValue, isolate))
    };

    v8::TryCatch exceptionCatcher;
    exceptionCatcher.SetVerbose(true);
    ScriptController::callFunctionWithInstrumentation(scriptExecutionContext(), callback, receiver, WTF_ARRAY_LENGTH(argv), argv, isolate);
}

void V8CustomElementLifecycleCallbacks::call(const ScopedPersistent<v8::Function>& weakCallback, Element* element)
{
    if (!canInvokeCallback())
        return;

    v8::HandleScope handleScope(isolateForScriptExecutionContext(scriptExecutionContext()));
    v8::Handle<v8::Context> context = toV8Context(scriptExecutionContext(), m_world.get());
    if (context.IsEmpty())
        return;

    v8::Context::Scope scope(context);
    v8::Isolate* isolate = context->GetIsolate();

    v8::Handle<v8::Function> callback = weakCallback.newLocal(isolate);
    if (callback.IsEmpty())
        return;

    v8::Handle<v8::Object> receiver = toV8(element, context->Global(), isolate).As<v8::Object>();
    ASSERT(!receiver.IsEmpty());

    v8::TryCatch exceptionCatcher;
    exceptionCatcher.SetVerbose(true);
    ScriptController::callFunctionWithInstrumentation(scriptExecutionContext(), callback, receiver, 0, 0, isolate);
}

} // namespace WebCore
