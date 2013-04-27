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

#include "V8HTMLElement.h"
#include "V8HTMLUnknownElement.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8DOMWrapper.h"
#include "core/dom/CustomElementConstructor.h"
#include "core/dom/CustomElementDefinition.h"
#include "core/dom/CustomElementRegistry.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"

namespace WebCore {

class CustomElementInvocation;
class Element;
class QualifiedName;
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

    static void invokeReadyCallbacksIfNeeded(ScriptExecutionContext*, const Vector<CustomElementInvocation>&);

    // You can just use toV8(Node*) to get correct wrapper objects,
    // even for custom elements.  Then generated
    // ElementWrapperFactories call V8CustomElement::wrap() with
    // proper prototype instances accordingly.
    static v8::Handle<v8::Object> wrap(Element*, v8::Handle<v8::Object> creationContext, v8::Isolate*);

    static bool hasDefinition(Element*);

private:
    static void invokeReadyCallbackIfNeeded(Element*, v8::Handle<v8::Context>);
    static v8::Handle<v8::Object> createWrapper(PassRefPtr<Element>, v8::Handle<v8::Object>, v8::Isolate*);
};

inline v8::Handle<v8::Object> CustomElementHelpers::wrap(Element* impl, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    ASSERT(impl);
    ASSERT(DOMDataStore::getWrapper(impl, isolate).IsEmpty());
    return CustomElementHelpers::createWrapper(impl, creationContext, isolate);
}

inline bool CustomElementHelpers::hasDefinition(Element* element)
{
    CustomElementRegistry* registry = element->document()->registry();
    if (registry && registry->findFor(element))
        return 1;
    return 0;
}

inline bool CustomElementHelpers::isValidPrototypeParameter(const ScriptValue& value, ScriptState* state)
{
    AtomicString namespaceURI;
    return isValidPrototypeParameter(value, state, namespaceURI);
}

} // namespace WebCore

#endif // CustomElementHelpers_h
