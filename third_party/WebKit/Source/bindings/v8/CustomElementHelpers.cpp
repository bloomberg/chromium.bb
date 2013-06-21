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

#include "V8HTMLElementWrapperFactory.h"
#include "V8SVGElementWrapperFactory.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/V8PerContextData.h"
#include "core/dom/CustomElementRegistry.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLUnknownElement.h"
#include "core/svg/SVGElement.h"

namespace WebCore {

v8::Handle<v8::Object> CustomElementHelpers::createWrapper(PassRefPtr<Element> impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate, const CreateWrapperFunction& createTypeExtensionUpgradeCandidateWrapper)
{
    ASSERT(impl);

    // FIXME: creationContext.IsEmpty() should never happen. Remove
    // this when callers (like InspectorController::inspect) are fixed
    // to never pass an empty creation context.
    v8::Handle<v8::Context> context = creationContext.IsEmpty() ? isolate->GetCurrentContext() : creationContext->CreationContext();

    // The constructor and registered lifecycle callbacks should be visible only from main world.
    // FIXME: This shouldn't be needed once each custom element has its own FunctionTemplate
    // https://bugs.webkit.org/show_bug.cgi?id=108138
    if (!CustomElementHelpers::isFeatureAllowed(context)) {
        v8::Handle<v8::Object> wrapper = V8DOMWrapper::createWrapper(creationContext, &V8HTMLElement::info, impl.get(), isolate);
        if (!wrapper.IsEmpty())
            V8DOMWrapper::associateObjectWithWrapper(impl, &V8HTMLElement::info, wrapper, isolate, WrapperConfiguration::Dependent);
        return wrapper;
    }

    CustomElementRegistry* registry = impl->document()->registry();
    RefPtr<CustomElementDefinition> definition = registry->findFor(impl.get());
    if (!definition)
        return createUpgradeCandidateWrapper(impl, creationContext, isolate, createTypeExtensionUpgradeCandidateWrapper);

    v8::Handle<v8::Object> prototype = V8PerContextData::from(context)->customElementPrototypes()->get(definition->type()).newLocal(isolate);
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
    } else if (element->isSVGElement()) {
        if (m_svg)
            return m_svg(toSVGElement(element), creationContext, isolate);
        return createV8SVGFallbackWrapper(toSVGElement(element), creationContext, isolate);
    }

    ASSERT(0);
    return v8::Handle<v8::Object>();
}

v8::Handle<v8::Object> CustomElementHelpers::createUpgradeCandidateWrapper(PassRefPtr<Element> element, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate, const CreateWrapperFunction& createTypeExtensionUpgradeCandidateWrapper)
{
    if (CustomElementRegistry::isCustomTagName(element->localName())) {
        if (element->isHTMLElement())
            return createV8HTMLDirectWrapper(toHTMLElement(element.get()), creationContext, isolate);
        else if (element->isSVGElement())
            return createV8SVGDirectWrapper(toSVGElement(element.get()), creationContext, isolate);
        else {
            ASSERT(0);
            return v8::Handle<v8::Object>();
        }
    } else {
        // It's a type extension
        return createTypeExtensionUpgradeCandidateWrapper.invoke(element.get(), creationContext, isolate);
    }
}

bool CustomElementHelpers::isFeatureAllowed(v8::Handle<v8::Context> context)
{
    if (DOMWrapperWorld* world = DOMWrapperWorld::isolatedWorld(context))
        return world->isMainWorld();
    return true;
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

} // namespace WebCore
