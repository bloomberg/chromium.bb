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

#include "config.h"

#include "bindings/v8/CustomElementHelpers.h"

#include "HTMLNames.h"
#include "V8CustomElementConstructor.h"
#include "V8HTMLElementWrapperFactory.h"
#include "bindings/v8/DOMDataStore.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScriptController.h"
#include "core/dom/CustomElementRegistry.h"
#include "core/dom/Node.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLUnknownElement.h"

#if ENABLE(SVG)
#include "V8SVGElementWrapperFactory.h"
#include "SVGNames.h"
#include "core/svg/SVGElement.h"
#endif

namespace WebCore {

v8::Handle<v8::Object> CustomElementHelpers::createWrapper(PassRefPtr<Element> impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate, const CreateWrapperFunction& createTypeExtensionUpgradeCandidateWrapper)
{
    ASSERT(impl);

    // The constructor and registered lifecycle callbacks should be visible only from main world.
    // FIXME: This shouldn't be needed once each custom element has its own FunctionTemplate
    // https://bugs.webkit.org/show_bug.cgi?id=108138

    // FIXME: creationContext.IsEmpty() should never happen. Remove
    // this when callers (like InspectorController::inspect) are fixed
    // to never pass an empty creation context.
    if (!CustomElementHelpers::isFeatureAllowed(creationContext.IsEmpty() ? v8::Context::GetCurrent() : creationContext->CreationContext())) {
        v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, &V8HTMLElement::info, impl.get(), isolate);
        if (!wrapper.IsEmpty())
            V8DOMWrapper::associateObjectWithWrapper(impl, &V8HTMLElement::info, wrapper, isolate, WrapperConfiguration::Dependent);
        return wrapper;
    }

    CustomElementRegistry* registry = impl->document()->registry();
    RefPtr<CustomElementDefinition> definition = registry->findFor(impl.get());
    if (!definition)
        return createUpgradeCandidateWrapper(impl, creationContext, isolate, createTypeExtensionUpgradeCandidateWrapper);

    v8::Handle<v8::Object> prototype = v8::Handle<v8::Object>::Cast(definition->prototype().v8Value());

    WrapperTypeInfo* typeInfo = CustomElementHelpers::findWrapperType(prototype);
    if (!typeInfo) {
        // FIXME: When can this happen?
        return v8::Handle<v8::Object>();
    }

    v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, typeInfo, impl.get(), isolate);
    if (wrapper.IsEmpty())
        return v8::Handle<v8::Object>();

    wrapper->SetPrototype(prototype);
    V8DOMWrapper::associateObjectWithWrapper(impl, typeInfo, wrapper, isolate, WrapperConfiguration::Dependent);
    return wrapper;
}

v8::Handle<v8::Object> CustomElementHelpers::CreateWrapperFunction::invoke(Element* element, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate) const
{
    if (element->isHTMLElement()) {
        if (m_html)
            return m_html(toHTMLElement(element), creationContext, isolate);
        return createV8HTMLFallbackWrapper(toHTMLUnknownElement(toHTMLElement(element)), creationContext, isolate);
    }
#if ENABLE(SVG)
    else if (element->isSVGElement()) {
        if (m_svg)
            return m_svg(toSVGElement(element), creationContext, isolate);
        return createV8SVGFallbackWrapper(toSVGElement(element), creationContext, isolate);
    }
#endif
    ASSERT(0);
    return v8::Handle<v8::Object>();
}

v8::Handle<v8::Object> CustomElementHelpers::createUpgradeCandidateWrapper(PassRefPtr<Element> element, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate, const CreateWrapperFunction& createTypeExtensionUpgradeCandidateWrapper)
{
    if (CustomElementRegistry::isCustomTagName(element->localName())) {
        if (element->isHTMLElement())
            return createV8HTMLDirectWrapper(toHTMLElement(element.get()), creationContext, isolate);
    #if ENABLE(SVG)
        else if (element->isSVGElement())
            return createV8SVGDirectWrapper(toSVGElement(element.get()), creationContext, isolate);
    #endif
        else {
            ASSERT(0);
            return v8::Handle<v8::Object>();
        }
    } else {
        // It's a type extension
        return createTypeExtensionUpgradeCandidateWrapper.invoke(element.get(), creationContext, isolate);
    }
}

bool CustomElementHelpers::initializeConstructorWrapper(CustomElementConstructor* constructor, const ScriptValue& prototype, ScriptState* state)
{
    ASSERT(isFeatureAllowed(state));
    ASSERT(!prototype.v8Value().IsEmpty() && prototype.v8Value()->IsObject());
    v8::Handle<v8::Value> wrapperValue = toV8(constructor, state->context()->Global(), state->context()->GetIsolate());
    if (wrapperValue.IsEmpty() || !wrapperValue->IsObject())
        return false;
    v8::Handle<v8::Function> wrapper = v8::Handle<v8::Function>::Cast(wrapperValue);
    // - Object::ForceSet() nor Object::SetAccessor Doesn't work against the "prototype" property of function objects.
    // - Set()-ing here is safe because
    //   - Hooking Object.prototype's defineProperty() with "prototype" or "constructor" also doesn't affect on these properties of function objects and
    //   - Using Set() is okay becaues each function has "prototype" property from start and Objects.prototype cannot intercept the property access.
    v8::Handle<v8::String> prototypeKey = v8String("prototype", state->context()->GetIsolate());
    ASSERT(wrapper->HasOwnProperty(prototypeKey));
    wrapper->Set(prototypeKey, prototype.v8Value(), v8::ReadOnly);

    v8::Handle<v8::String> constructorKey = v8String("constructor", state->context()->GetIsolate());
    v8::Handle<v8::Object> prototypeObject = v8::Handle<v8::Object>::Cast(prototype.v8Value());
    ASSERT(!prototypeObject->HasOwnProperty(constructorKey));
    prototypeObject->ForceSet(constructorKey, wrapper, v8::ReadOnly);
    return true;
}

static bool hasValidPrototypeChainFor(v8::Handle<v8::Object> prototypeObject, WrapperTypeInfo* typeInfo, v8::Handle<v8::Context> context)
{
    // document.register() sets the constructor property, so the prototype shouldn't have one.
    if (prototypeObject->HasOwnProperty(v8String("constructor", context->GetIsolate())))
        return false;

    v8::Handle<v8::Object> elementConstructor = v8::Handle<v8::Object>::Cast(V8PerContextData::from(context)->constructorForType(typeInfo));
    if (elementConstructor.IsEmpty())
        return false;
    v8::Handle<v8::Object> elementPrototype = v8::Handle<v8::Object>::Cast(elementConstructor->Get(v8String("prototype", context->GetIsolate())));
    if (elementPrototype.IsEmpty())
        return false;

    v8::Handle<v8::Value> chain = prototypeObject;
    while (!chain.IsEmpty() && chain->IsObject()) {
        if (chain == elementPrototype)
            return true;
        chain = v8::Handle<v8::Object>::Cast(chain)->GetPrototype();
    }

    return false;
}

bool CustomElementHelpers::isValidPrototypeParameter(const ScriptValue& prototype, ScriptState* state, AtomicString& namespaceURI)
{
    if (prototype.v8Value().IsEmpty() || !prototype.v8Value()->IsObject())
        return false;

    v8::Handle<v8::Object> prototypeObject = v8::Handle<v8::Object>::Cast(prototype.v8Value());
    if (hasValidPrototypeChainFor(prototypeObject, &V8HTMLElement::info, state->context())) {
        namespaceURI = HTMLNames::xhtmlNamespaceURI;
        return true;
    }

#if ENABLE(SVG)
    if (hasValidPrototypeChainFor(prototypeObject, &V8SVGElement::info, state->context())) {
        namespaceURI = SVGNames::svgNamespaceURI;
        return true;
    }
#endif

    if (hasValidPrototypeChainFor(prototypeObject, &V8Element::info, state->context())) {
        namespaceURI = nullAtom;
        return true;
    }

    return false;
}

bool CustomElementHelpers::isFeatureAllowed(ScriptState* state)
{
    return isFeatureAllowed(state->context());
}

bool CustomElementHelpers::isFeatureAllowed(v8::Handle<v8::Context> context)
{
    if (DOMWrapperWorld* world = DOMWrapperWorld::isolatedWorld(context))
        return world->isMainWorld();
    return true;
}

const QualifiedName* CustomElementHelpers::findLocalName(const ScriptValue& prototype)
{
    if (prototype.v8Value().IsEmpty() || !prototype.v8Value()->IsObject())
        return 0;
    return findLocalName(v8::Handle<v8::Object>::Cast(prototype.v8Value()));
}

WrapperTypeInfo* CustomElementHelpers::findWrapperType(v8::Handle<v8::Value> chain)
{
    while (!chain.IsEmpty() && chain->IsObject()) {
        v8::Handle<v8::Object> chainObject = v8::Handle<v8::Object>::Cast(chain);
        // Only prototype objects of native-backed types have the extra internal field storing WrapperTypeInfo.
        if (v8PrototypeInternalFieldcount == chainObject->InternalFieldCount())
            return reinterpret_cast<WrapperTypeInfo*>(chainObject->GetAlignedPointerFromInternalField(v8PrototypeTypeIndex));
        chain = chainObject->GetPrototype();
    }

    return 0;
}

// This can return null. In that case, we should take the element name as its local name.
const QualifiedName* CustomElementHelpers::findLocalName(v8::Handle<v8::Object> chain)
{
    WrapperTypeInfo* type = CustomElementHelpers::findWrapperType(chain);
    if (!type)
        return 0;
    if (const QualifiedName* htmlName = findHTMLTagNameOfV8Type(type))
        return htmlName;
#if ENABLE(SVG)
    if (const QualifiedName* svgName = findSVGTagNameOfV8Type(type))
        return svgName;
#endif
    return 0;
}

void CustomElementHelpers::upgradeWrappers(ScriptExecutionContext* executionContext, const HashSet<Element*>& elements, const ScriptValue& prototype)
{
    if (elements.isEmpty())
        return;

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = toV8Context(executionContext, mainThreadNormalWorld());
    v8::Context::Scope scope(context);

    v8::Handle<v8::Value> v8Prototype = prototype.v8Value();

    for (HashSet<Element*>::const_iterator it = elements.begin(); it != elements.end(); ++it) {
        v8::Handle<v8::Object> wrapper = DOMDataStore::getWrapperForMainWorld(*it);
        if (wrapper.IsEmpty()) {
            // The wrapper will be created with the right prototype when
            // retrieved; we don't need to eagerly create the wrapper.
            continue;
        }
        wrapper->SetPrototype(v8Prototype);
    }
}

void CustomElementHelpers::invokeReadyCallbackIfNeeded(Element* element, v8::Handle<v8::Context> context)
{
    v8::Handle<v8::Value> wrapperValue = toV8(element, context->Global(), context->GetIsolate());
    if (wrapperValue.IsEmpty() || !wrapperValue->IsObject())
        return;
    v8::Handle<v8::Object> wrapper = v8::Handle<v8::Object>::Cast(wrapperValue);
    v8::Handle<v8::Value> prototypeValue = wrapper->GetPrototype();
    if (prototypeValue.IsEmpty() || !prototypeValue->IsObject())
        return;
    v8::Handle<v8::Object> prototype = v8::Handle<v8::Object>::Cast(prototypeValue);
    v8::Handle<v8::Value> functionValue = prototype->Get(v8::String::NewSymbol("readyCallback"));
    if (functionValue.IsEmpty() || !functionValue->IsFunction())
        return;

    v8::Handle<v8::Function> function = v8::Handle<v8::Function>::Cast(functionValue);
    v8::TryCatch exceptionCatcher;
    exceptionCatcher.SetVerbose(true);
    v8::Handle<v8::Value> args[] = { v8::Handle<v8::Value>() };
    ScriptController::callFunctionWithInstrumentation(element->document(), function, wrapper, 0, args);
}


void CustomElementHelpers::invokeReadyCallbacksIfNeeded(ScriptExecutionContext* executionContext, const Vector<CustomElementInvocation>& invocations)
{
    ASSERT(!invocations.isEmpty());

    v8::HandleScope handleScope;
    v8::Handle<v8::Context> context = toV8Context(executionContext, mainThreadNormalWorld());
    if (context.IsEmpty())
        return;
    v8::Context::Scope scope(context);

    for (size_t i = 0; i < invocations.size(); ++i) {
        ASSERT(executionContext == invocations[i].element()->document());
        invokeReadyCallbackIfNeeded(invocations[i].element(), context);
    }
}

} // namespace WebCore
