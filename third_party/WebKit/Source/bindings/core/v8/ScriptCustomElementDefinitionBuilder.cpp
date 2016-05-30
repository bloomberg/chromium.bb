// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptCustomElementDefinitionBuilder.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptCustomElementDefinition.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

ScriptCustomElementDefinitionBuilder::ScriptCustomElementDefinitionBuilder(
    ScriptState* scriptState,
    CustomElementsRegistry* registry,
    const ScriptValue& constructor,
    ExceptionState& exceptionState)
    : m_scriptState(scriptState)
    , m_registry(registry)
    , m_constructorValue(constructor.v8Value())
    , m_exceptionState(exceptionState)
{
}

bool ScriptCustomElementDefinitionBuilder::checkConstructorIntrinsics()
{
    DCHECK(m_scriptState->world().isMainWorld());

    // The signature of CustomElementsRegistry.define says this is a
    // Function
    // https://html.spec.whatwg.org/multipage/scripting.html#customelementsregistry
    CHECK(m_constructorValue->IsFunction());
    m_constructor = m_constructorValue.As<v8::Object>();
    if (!m_constructor->IsConstructor()) {
        m_exceptionState.throwTypeError(
            "constructor argument is not a constructor");
        return false;
    }
    return true;
}

bool ScriptCustomElementDefinitionBuilder::checkConstructorNotRegistered()
{
    if (ScriptCustomElementDefinition::forConstructor(
        m_scriptState.get(),
        m_registry,
        m_constructor)) {

        // Constructor is already registered.
        m_exceptionState.throwDOMException(
            NotSupportedError,
            "this constructor has already been used with this registry");
        return false;
    }
    return true;
}

bool ScriptCustomElementDefinitionBuilder::checkPrototype()
{
    v8::Isolate* isolate = m_scriptState->isolate();
    v8::Local<v8::Context> context = m_scriptState->context();
    v8::Local<v8::String> prototypeString =
        v8AtomicString(isolate, "prototype");
    v8::Local<v8::Value> prototypeValue;
    if (!v8Call(
        m_constructor->Get(context, prototypeString), prototypeValue)) {
        return false;
    }
    if (!prototypeValue->IsObject()) {
        m_exceptionState.throwTypeError(
            "constructor prototype is not an object");
        return false;
    }
    m_prototype = prototypeValue.As<v8::Object>();
    // If retrieving the prototype destroyed the context, indicate that
    // defining the element should not proceed.
    return true;
}

CustomElementDefinition* ScriptCustomElementDefinitionBuilder::build(
    const CustomElementDescriptor& descriptor)
{
    return ScriptCustomElementDefinition::create(
        m_scriptState.get(),
        m_registry,
        descriptor,
        m_constructor,
        m_prototype);
}

} // namespace blink
