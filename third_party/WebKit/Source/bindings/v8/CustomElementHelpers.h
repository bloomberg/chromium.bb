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

#ifndef CustomElementHelpers_h
#define CustomElementHelpers_h

#include "bindings/v8/DOMDataStore.h"
#include "bindings/v8/ScriptValue.h"
#include "core/dom/CustomElementDefinition.h"
#include "core/dom/CustomElementRegistry.h"
#include "core/dom/Element.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

class CustomElementConstructor;
class CustomElementInvocation;
class HTMLElement;
class QualifiedName;
class SVGElement;
class ScriptState;

class CustomElementHelpers {
public:
    static bool initializeConstructorWrapper(CustomElementConstructor*, const ScriptValue& prototype, ScriptState*);
    static bool isValidPrototypeParameter(const ScriptValue&, ScriptState*, AtomicString& namespaceURI);
    static bool isValidPrototypeParameter(const ScriptValue&, ScriptState*);
    static bool isFeatureAllowed(ScriptState*);
    static const QualifiedName* findLocalName(const ScriptValue& prototype);

    static bool isFeatureAllowed(v8::Handle<v8::Context>);
    static WrapperTypeInfo* findWrapperType(v8::Handle<v8::Value> chain);
    static const QualifiedName* findLocalName(v8::Handle<v8::Object> chain);

    static void upgradeWrappers(ScriptExecutionContext*, const HashSet<Element*>&, const ScriptValue& prototype);
    static void invokeReadyCallbacksIfNeeded(ScriptExecutionContext*, const Vector<CustomElementInvocation>&);

    typedef v8::Handle<v8::Object> (*CreateSVGWrapperFunction)(SVGElement*, v8::Handle<v8::Object> creationContext, v8::Isolate*);
    typedef v8::Handle<v8::Object> (*CreateHTMLWrapperFunction)(HTMLElement*, v8::Handle<v8::Object> creationContext, v8::Isolate*);

    // CustomElementHelpers::wrap is a factory for both HTMLElement
    // and SVGElement wrappers. CreateWrapperFunction is a type safe
    // way of passing a wrapping function for specific elements of
    // either type; it's used as a fallback when creating wrappers for
    // type extensions.
    class CreateWrapperFunction {
    public:
        explicit CreateWrapperFunction(CreateSVGWrapperFunction svg)
            : m_svg(svg) { }
        explicit CreateWrapperFunction(CreateHTMLWrapperFunction html)
            : m_html(html) { }
        v8::Handle<v8::Object> invoke(Element*, v8::Handle<v8::Object> creationContext, v8::Isolate*) const;
    private:
        CreateSVGWrapperFunction m_svg;
        CreateHTMLWrapperFunction m_html;
    };

    // You can just use toV8(Node*) to get correct wrapper objects,
    // even for custom elements.  Then generated
    // ElementWrapperFactories call V8CustomElement::wrap() with
    // proper prototype instances accordingly.
    static v8::Handle<v8::Object> wrap(Element*, v8::Handle<v8::Object> creationContext, v8::Isolate*, const CreateWrapperFunction& createTypeExtensionUpgradeCandidateWrapper);

private:
    static void invokeReadyCallbackIfNeeded(Element*, v8::Handle<v8::Context>);
    static v8::Handle<v8::Object> createWrapper(PassRefPtr<Element>, v8::Handle<v8::Object>, v8::Isolate*, const CreateWrapperFunction& createTypeExtensionUpgradeCandidateWrapper);
    static v8::Handle<v8::Object> createUpgradeCandidateWrapper(PassRefPtr<Element>, v8::Handle<v8::Object> creationContext, v8::Isolate*, const CreateWrapperFunction& createTypeExtensionUpgradeCandidateWrapper);
};

inline v8::Handle<v8::Object> CustomElementHelpers::wrap(Element* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate, const CreateWrapperFunction& createWrapper)
{
    ASSERT(impl);
    ASSERT(DOMDataStore::getWrapper(impl, isolate).IsEmpty());
    return CustomElementHelpers::createWrapper(impl, creationContext, isolate, createWrapper);
}

inline bool CustomElementHelpers::isValidPrototypeParameter(const ScriptValue& value, ScriptState* state)
{
    AtomicString namespaceURI;
    return isValidPrototypeParameter(value, state, namespaceURI);
}

} // namespace WebCore

#endif // CustomElementHelpers_h
