// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ScriptCustomElementDefinition.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8CustomElementsRegistry.h"
#include "bindings/core/v8/V8Element.h"
#include "bindings/core/v8/V8HiddenValue.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8ThrowException.h"
#include "core/dom/ExceptionCode.h"
#include "v8.h"
#include "wtf/Allocator.h"

namespace blink {

// Retrieves the custom elements constructor -> name map, creating it
// if necessary. The same map is used to keep prototypes alive.
static v8::Local<v8::Map> ensureCustomElementsRegistryMap(
    ScriptState* scriptState,
    CustomElementsRegistry* registry)
{
    CHECK(scriptState->world().isMainWorld());
    v8::Local<v8::String> name = V8HiddenValue::customElementsRegistryMap(
        scriptState->isolate());
    v8::Local<v8::Object> wrapper =
        toV8(registry, scriptState).As<v8::Object>();
    v8::Local<v8::Value> map =
        V8HiddenValue::getHiddenValue(scriptState, wrapper, name);
    if (map.IsEmpty()) {
        map = v8::Map::New(scriptState->isolate());
        V8HiddenValue::setHiddenValue(scriptState, wrapper, name, map);
    }
    return map.As<v8::Map>();
}

ScriptCustomElementDefinition* ScriptCustomElementDefinition::forConstructor(
    ScriptState* scriptState,
    CustomElementsRegistry* registry,
    const v8::Local<v8::Value>& constructor)
{
    v8::Local<v8::Map> map =
        ensureCustomElementsRegistryMap(scriptState, registry);
    v8::Local<v8::Value> nameValue = v8CallOrCrash(
        map->Get(scriptState->context(), constructor));
    if (!nameValue->IsString())
        return nullptr;
    AtomicString name = toCoreAtomicString(nameValue.As<v8::String>());

    // This downcast is safe because only
    // ScriptCustomElementDefinitions have a name associated with a V8
    // constructor in the map; see
    // ScriptCustomElementDefinition::create. This relies on three
    // things:
    //
    // 1. Only ScriptCustomElementDefinition adds entries to the map.
    //    Audit the use of V8HiddenValue/hidden values in general and
    //    how the map is handled--it should never be leaked to script.
    //
    // 2. CustomElementsRegistry does not overwrite definitions with a
    //    given name--see the CHECK in CustomElementsRegistry::define
    //    --and adds ScriptCustomElementDefinitions to the map without
    //    fail.
    //
    // 3. The relationship between the CustomElementsRegistry and its
    //    map is never mixed up; this is guaranteed by the bindings
    //    system which provides a stable wrapper, and the map hangs
    //    off the wrapper.
    //
    // At a meta-level, this downcast is safe because there is
    // currently only one implementation of CustomElementDefinition in
    // product code and that is ScriptCustomElementDefinition. But
    // that may change in the future.
    CustomElementDefinition* definition = registry->definitionForName(name);
    CHECK(definition);
    return static_cast<ScriptCustomElementDefinition*>(definition);
}

ScriptCustomElementDefinition* ScriptCustomElementDefinition::create(
    ScriptState* scriptState,
    CustomElementsRegistry* registry,
    const CustomElementDescriptor& descriptor,
    const v8::Local<v8::Object>& constructor,
    const v8::Local<v8::Object>& prototype)
{
    ScriptCustomElementDefinition* definition =
        new ScriptCustomElementDefinition(
            scriptState,
            descriptor,
            constructor,
            prototype);

    // Add a constructor -> name mapping to the registry.
    v8::Local<v8::Value> nameValue =
        v8String(scriptState->isolate(), descriptor.name());
    v8::Local<v8::Map> map =
        ensureCustomElementsRegistryMap(scriptState, registry);
    v8CallOrCrash(map->Set(scriptState->context(), constructor, nameValue));
    // We add the prototype here to keep it alive; we make it a value
    // not a key so authors cannot return another constructor as a
    // prototype to overwrite a constructor in this map. We use the
    // name because it is unique per-registry.
    v8CallOrCrash(map->Set(scriptState->context(), nameValue, prototype));

    return definition;
}

ScriptCustomElementDefinition::ScriptCustomElementDefinition(
    ScriptState* scriptState,
    const CustomElementDescriptor& descriptor,
    const v8::Local<v8::Object>& constructor,
    const v8::Local<v8::Object>& prototype)
    : CustomElementDefinition(descriptor)
    , m_scriptState(scriptState)
    , m_constructor(scriptState->isolate(), constructor)
    , m_prototype(scriptState->isolate(), prototype)
{
    // These objects are kept alive by references from the
    // CustomElementsRegistry wrapper set up by
    // ScriptCustomElementDefinition::create.
    m_constructor.setPhantom();
    m_prototype.setPhantom();
}

// https://html.spec.whatwg.org/multipage/scripting.html#upgrades
bool ScriptCustomElementDefinition::runConstructor(Element* element)
{
    if (!m_scriptState->contextIsValid())
        return false;
    ScriptState::Scope scope(m_scriptState.get());
    v8::Isolate* isolate = m_scriptState->isolate();

    // Step 5 says to rethrow the exception; but there is no one to
    // catch it. The side effect is to report the error.
    v8::TryCatch tryCatch(isolate);
    tryCatch.SetVerbose(true);

    ExecutionContext* executionContext = m_scriptState->getExecutionContext();
    v8::Local<v8::Value> result;
    if (!v8Call(V8ScriptRunner::callAsConstructor(
        isolate,
        constructor(),
        executionContext,
        0,
        nullptr),
        result))
        return false;

    if (V8Element::toImplWithTypeCheck(isolate, result) != element) {
        V8ThrowException::throwException(
            V8ThrowException::createDOMException(
                m_scriptState->isolate(),
                InvalidStateError,
                "custom element constructors must call super() first and must "
                "not return a different object",
                constructor()),
            m_scriptState->isolate());
        return false;
    }

    return true;
}

v8::Local<v8::Object> ScriptCustomElementDefinition::constructor() const
{
    DCHECK(!m_constructor.isEmpty());
    return m_constructor.newLocal(m_scriptState->isolate());
}

v8::Local<v8::Object> ScriptCustomElementDefinition::prototype() const
{
    DCHECK(!m_prototype.isEmpty());
    return m_prototype.newLocal(m_scriptState->isolate());
}

// CustomElementDefinition
ScriptValue ScriptCustomElementDefinition::getConstructorForScript()
{
    return ScriptValue(m_scriptState.get(), constructor());
}

} // namespace blink
