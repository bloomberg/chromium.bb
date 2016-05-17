// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElementsRegistry.h"

// TODO(dominicc): Stop including Document.h when
// v0CustomElementIsDefined has been removed.
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "core/dom/Document.h"
#include "core/dom/ElementRegistrationOptions.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/custom/CustomElement.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "core/dom/custom/V0CustomElementRegistrationContext.h"
#include "core/dom/custom/V0CustomElementRegistry.h"

namespace blink {

CustomElementsRegistry* CustomElementsRegistry::create(
    ScriptState* scriptState,
    V0CustomElementRegistrationContext* v0)
{
    DCHECK(scriptState->world().isMainWorld());
    CustomElementsRegistry* registry = new CustomElementsRegistry(v0);
    if (v0)
        v0->setV1(registry);

    v8::Isolate* isolate = scriptState->isolate();
    v8::Local<v8::Object> wrapper =
        toV8(registry, scriptState).As<v8::Object>();
    v8::Local<v8::String> name =
        V8HiddenValue::customElementsRegistryMap(isolate);
    v8::Local<v8::Map> map = v8::Map::New(isolate);
    bool didSetPrototype =
        V8HiddenValue::setHiddenValue(scriptState, wrapper, name, map);
    DCHECK(didSetPrototype);

    return registry;
}

CustomElementsRegistry::CustomElementsRegistry(
    const V0CustomElementRegistrationContext* v0)
    : m_v0(v0)
{
}

// http://w3c.github.io/webcomponents/spec/custom/#dfn-element-definition
void CustomElementsRegistry::define(ScriptState* scriptState,
    const AtomicString& name, const ScriptValue& constructorScriptValue,
    const ElementRegistrationOptions& options, ExceptionState& exceptionState)
{
    DCHECK(scriptState->world().isMainWorld());
    v8::Isolate* isolate = scriptState->isolate();
    v8::Local<v8::Context> context = scriptState->context();

    v8::Local<v8::Value> constructorValue = constructorScriptValue.v8Value();
    if (!constructorValue->IsFunction()) {
        // Not even a function.
        exceptionState.throwTypeError(
            "constructor argument is not a constructor");
        return;
    }
    v8::Local<v8::Object> constructor = constructorValue.As<v8::Object>();
    if (!constructor->IsConstructor()) {
        exceptionState.throwTypeError(
            "constructor argument is not a constructor");
        return;
    }

    // Raise an exception if the name is not valid.
    if (!CustomElement::isValidName(name)) {
        exceptionState.throwDOMException(
            SyntaxError,
            "\"" + name + "\" is not a valid custom element name");
        return;
    }

    // Raise an exception if the name is already in use.
    if (nameIsDefined(name) || v0NameIsDefined(name)) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "this name has already been used with this registry");
        return;
    }

    // Raise an exception if the constructor is already registered.
    if (definitionForConstructor(scriptState, constructor)) {
        exceptionState.throwDOMException(
            NotSupportedError,
            "this constructor has already been used with this registry");
        return;
    }

    // TODO(dominicc): Implement steps:
    // 5: localName
    // 6-7: extends processing
    // 8-9: observed attributes caching

    // TODO(dominicc): Add a test where the prototype getter destroys
    // the context.

    v8::TryCatch tryCatch(isolate);
    v8::Local<v8::String> prototypeString =
        v8AtomicString(isolate, "prototype");
    v8::Local<v8::Value> prototypeValue;
    if (!v8Call(
        constructor->Get(context, prototypeString), prototypeValue)) {
        DCHECK(tryCatch.HasCaught());
        tryCatch.ReThrow();
        return;
    }
    if (!prototypeValue->IsObject()) {
        DCHECK(!tryCatch.HasCaught());
        exceptionState.throwTypeError("constructor prototype is not an object");
        return;
    }
    v8::Local<v8::Object> prototype = prototypeValue.As<v8::Object>();

    // TODO(dominicc): Implement steps:
    // 12-13: connected callback
    // 14-15: disconnected callback
    // 16-17: attribute changed callback

    Id id = m_definitions.size();
    v8::Local<v8::Value> idValue = v8::Integer::NewFromUnsigned(isolate, id);
    m_definitions.append(new CustomElementDefinition(this, id, name));
    // This map is stored in a hidden reference from the
    // CustomElementsRegistry wrapper.
    v8::Local<v8::Map> map = idMap(scriptState);
    // The map keeps the constructor and prototypes alive.
    v8CallOrCrash(map->Set(context, constructor, idValue));
    v8CallOrCrash(map->Set(context, idValue, prototype));
    m_names.add(name);

    // TODO(dominicc): Implement steps:
    // 20: when-defined promise processing
    DCHECK(!tryCatch.HasCaught() || tryCatch.HasTerminated());
}

CustomElementDefinition* CustomElementsRegistry::definitionForConstructor(
    ScriptState* scriptState,
    v8::Local<v8::Value> constructor)
{
    Id id;
    if (!idForConstructor(scriptState, constructor, id))
        return nullptr;
    return m_definitions[id];
}

v8::Local<v8::Object> CustomElementsRegistry::prototype(
    ScriptState* scriptState,
    const CustomElementDefinition& def)
{
    v8::Local<v8::Value> idValue =
        v8::Integer::NewFromUnsigned(scriptState->isolate(), def.id());
    return v8CallOrCrash(
        idMap(scriptState)->Get(scriptState->context(), idValue))
        .As<v8::Object>();
}

bool CustomElementsRegistry::nameIsDefined(const AtomicString& name) const
{
    return m_names.contains(name);
}

v8::Local<v8::Map> CustomElementsRegistry::idMap(ScriptState* scriptState)
{
    DCHECK(scriptState->world().isMainWorld());
    v8::Local<v8::Object> wrapper =
        toV8(this, scriptState).As<v8::Object>();
    v8::Local<v8::String> name = V8HiddenValue::customElementsRegistryMap(
        scriptState->isolate());
    return V8HiddenValue::getHiddenValue(scriptState, wrapper, name)
        .As<v8::Map>();
}

bool CustomElementsRegistry::idForConstructor(
    ScriptState* scriptState,
    v8::Local<v8::Value> constructor,
    Id& id)
{
    v8::Local<v8::Value> entry = v8CallOrCrash(
        idMap(scriptState)->Get(scriptState->context(), constructor));
    if (!entry->IsUint32())
        return false;
    id = v8CallOrCrash(entry->Uint32Value(scriptState->context()));
    return true;
}

bool CustomElementsRegistry::v0NameIsDefined(const AtomicString& name)
{
    return m_v0.get() && m_v0->nameIsDefined(name);
}

DEFINE_TRACE(CustomElementsRegistry)
{
    visitor->trace(m_definitions);
    visitor->trace(m_v0);
}

} // namespace blink
