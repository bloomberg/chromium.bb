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
#include "bindings/v8/CustomElementConstructorBuilder.h"

#include "HTMLNames.h"
#include "SVGNames.h"
#include "V8Document.h"
#include "V8HTMLElementWrapperFactory.h"
#include "V8SVGElementWrapperFactory.h"
#include "bindings/v8/CustomElementHelpers.h"
#include "bindings/v8/Dictionary.h"
#include "bindings/v8/UnsafePersistent.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8CustomElementCallback.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "bindings/v8/V8PerContextData.h"
#include "core/dom/CustomElementDefinition.h"
#include "core/dom/CustomElementRegistry.h"
#include "core/dom/Document.h"
#include "wtf/Assertions.h"
#include "wtf/RefPtr.h"

namespace WebCore {

static void constructCustomElement(const v8::FunctionCallbackInfo<v8::Value>&);

CustomElementConstructorBuilder::CustomElementConstructorBuilder(ScriptState* state, const Dictionary* options)
    : m_context(state->context())
    , m_options(options)
{
    ASSERT(m_context == v8::Isolate::GetCurrent()->GetCurrentContext());
}

bool CustomElementConstructorBuilder::isFeatureAllowed() const
{
    return CustomElementHelpers::isFeatureAllowed(m_context);
}

bool CustomElementConstructorBuilder::validateOptions()
{
    ASSERT(m_prototype.IsEmpty());

    ScriptValue prototypeScriptValue;
    if (!m_options->get("prototype", prototypeScriptValue)) {
        // FIXME: Implement the default value handling.
        // Currently default value of the "prototype" parameter, which
        // is HTMLSpanElement.prototype, has an ambiguity about its
        // behavior. The spec should be fixed before WebKit implements
        // it. https://www.w3.org/Bugs/Public/show_bug.cgi?id=20801
        return false;
    }

    v8::Handle<v8::Value> prototypeValue = prototypeScriptValue.v8Value();
    if (prototypeValue.IsEmpty() || !prototypeValue->IsObject())
        return false;

    m_prototype = v8::Handle<v8::Object>::Cast(prototypeValue);
    if (hasValidPrototypeChainFor(&V8HTMLElement::info)) {
        m_namespaceURI = HTMLNames::xhtmlNamespaceURI;
        return true;
    }

    if (hasValidPrototypeChainFor(&V8SVGElement::info)) {
        m_namespaceURI = SVGNames::svgNamespaceURI;
        return true;
    }

    if (hasValidPrototypeChainFor(&V8Element::info)) {
        m_namespaceURI = nullAtom;
        // This generates a different DOM exception, so we feign success for now.
        return true;
    }

    return false;
}

bool CustomElementConstructorBuilder::findTagName(const AtomicString& customElementType, QualifiedName& tagName) const
{
    ASSERT(!m_prototype.IsEmpty());

    WrapperTypeInfo* wrapperTypeInfo = CustomElementHelpers::findWrapperType(m_prototype);
    if (!wrapperTypeInfo) {
        // Invalid prototype.
        return false;
    }

    if (const QualifiedName* htmlName = findHTMLTagNameOfV8Type(wrapperTypeInfo)) {
        ASSERT(htmlName->namespaceURI() == m_namespaceURI);
        tagName = *htmlName;
        return true;
    }

    if (const QualifiedName* svgName = findSVGTagNameOfV8Type(wrapperTypeInfo)) {
        ASSERT(svgName->namespaceURI() == m_namespaceURI);
        tagName = *svgName;
        return true;
    }

    if (m_namespaceURI != nullAtom) {
        // Use the custom element type as the tag's local name.
        tagName = QualifiedName(nullAtom, customElementType, m_namespaceURI);
        return true;
    }

    return false;
}

PassRefPtr<CustomElementCallback> CustomElementConstructorBuilder::createCallback(Document* document)
{
    ASSERT(!m_prototype.IsEmpty());

    RefPtr<Document> protect(document);

    v8::TryCatch exceptionCatcher;
    exceptionCatcher.SetVerbose(true);

    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::Handle<v8::Value> readyValue = m_prototype->Get(v8String("readyCallback", isolate));

    v8::Handle<v8::Function> readyFunction;
    if (!readyValue.IsEmpty() && readyValue->IsFunction())
        readyFunction = v8::Handle<v8::Function>::Cast(readyValue);

    return V8CustomElementCallback::create(document, m_prototype, readyFunction);
}

bool CustomElementConstructorBuilder::createConstructor(Document* document, CustomElementDefinition* definition)
{
    ASSERT(!m_prototype.IsEmpty());
    ASSERT(m_constructor.IsEmpty());
    ASSERT(document);

    v8::Isolate* isolate = m_context->GetIsolate();

    if (!prototypeIsValid())
        return false;

    v8::Local<v8::FunctionTemplate> constructorTemplate = v8::FunctionTemplate::New();
    constructorTemplate->SetCallHandler(constructCustomElement);
    m_constructor = constructorTemplate->GetFunction();
    if (m_constructor.IsEmpty())
        return false;

    v8::Handle<v8::String> v8Name = v8String(definition->name(), isolate);
    v8::Handle<v8::Value> v8Type = v8StringOrNull(definition->isTypeExtension() ? definition->type() : nullAtom, isolate);

    m_constructor->SetName(v8Type->IsNull() ? v8Name : v8::Handle<v8::String>::Cast(v8Type));

    V8HiddenPropertyName::setNamedHiddenReference(m_constructor, "document", toV8(document, m_context->Global(), isolate));
    V8HiddenPropertyName::setNamedHiddenReference(m_constructor, "namespaceURI", v8String(definition->namespaceURI(), isolate));
    V8HiddenPropertyName::setNamedHiddenReference(m_constructor, "name", v8Name);
    V8HiddenPropertyName::setNamedHiddenReference(m_constructor, "type", v8Type);

    v8::Handle<v8::String> prototypeKey = v8String("prototype", isolate);
    ASSERT(m_constructor->HasOwnProperty(prototypeKey));
    // This sets the property *value*; calling Set is safe because
    // "prototype" is a non-configurable data property so there can be
    // no side effects.
    m_constructor->Set(prototypeKey, m_prototype);
    // This *configures* the property. ForceSet of a function's
    // "prototype" does not affect the value, but can reconfigure the
    // property.
    m_constructor->ForceSet(prototypeKey, m_prototype, v8::PropertyAttribute(v8::ReadOnly | v8::DontEnum | v8::DontDelete));

    V8HiddenPropertyName::setNamedHiddenReference(m_prototype, "isCustomElementInterfacePrototypeObject", v8::True());
    m_prototype->ForceSet(v8String("constructor", isolate), m_constructor, v8::DontEnum);

    return true;
}

bool CustomElementConstructorBuilder::prototypeIsValid() const
{
    if (m_prototype->InternalFieldCount() || !m_prototype->GetHiddenValue(V8HiddenPropertyName::isCustomElementInterfacePrototypeObject()).IsEmpty()) {
        // Already an interface prototype object.
        return false;
    }

    if (m_prototype->GetPropertyAttributes(v8String("constructor", m_context->GetIsolate())) & v8::DontDelete) {
        // "constructor" is not configurable.
        return false;
    }

    return true;
}

void CustomElementConstructorBuilder::didRegisterDefinition(CustomElementDefinition* definition, const HashSet<Element*>& upgradeCandidates) const
{
    ASSERT(!m_constructor.IsEmpty());

    // Bindings retrieve the prototype when needed from per-context data.
    v8::Persistent<v8::Object> persistentPrototype(m_context->GetIsolate(), m_prototype);
    V8PerContextData::from(m_context)->customElementPrototypes()->add(definition->type(), UnsafePersistent<v8::Object>(persistentPrototype));

    // Upgrade any wrappers already created for this definition
    for (HashSet<Element*>::const_iterator it = upgradeCandidates.begin(); it != upgradeCandidates.end(); ++it) {
        v8::Handle<v8::Object> wrapper = DOMDataStore::getWrapperForMainWorld(*it);
        if (wrapper.IsEmpty()) {
            // The wrapper will be created with the right prototype when
            // retrieved; we don't need to eagerly create the wrapper.
            continue;
        }
        wrapper->SetPrototype(m_prototype);
    }
}

ScriptValue CustomElementConstructorBuilder::bindingsReturnValue() const
{
    return ScriptValue(m_constructor);
}

bool CustomElementConstructorBuilder::hasValidPrototypeChainFor(WrapperTypeInfo* typeInfo) const
{
    v8::Handle<v8::Object> elementConstructor = v8::Handle<v8::Object>::Cast(V8PerContextData::from(m_context)->constructorForType(typeInfo));
    if (elementConstructor.IsEmpty())
        return false;
    v8::Handle<v8::Object> elementPrototype = v8::Handle<v8::Object>::Cast(elementConstructor->Get(v8String("prototype", m_context->GetIsolate())));
    if (elementPrototype.IsEmpty())
        return false;

    v8::Handle<v8::Value> chain = m_prototype;
    while (!chain.IsEmpty() && chain->IsObject()) {
        if (chain == elementPrototype)
            return true;
        chain = v8::Handle<v8::Object>::Cast(chain)->GetPrototype();
    }

    return false;
}

static void constructCustomElement(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    v8::Isolate* isolate = args.GetIsolate();

    if (!args.IsConstructCall()) {
        throwTypeError("DOM object constructor cannot be called as a function.", isolate);
        return;
    }

    if (args.Length() > 0) {
        throwTypeError(0, isolate);
        return;
    }

    Document* document = V8Document::toNative(v8::Handle<v8::Object>::Cast(args.Callee()->GetHiddenValue(V8HiddenPropertyName::document())));
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, namespaceURI, args.Callee()->GetHiddenValue(V8HiddenPropertyName::namespaceURI()));
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, name, args.Callee()->GetHiddenValue(V8HiddenPropertyName::name()));
    v8::Handle<v8::Value> maybeType = args.Callee()->GetHiddenValue(V8HiddenPropertyName::type());
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, type, maybeType);

    ExceptionCode ec = 0;
    CustomElementRegistry::CallbackDeliveryScope deliveryScope;
    RefPtr<Element> element = document->createElementNS(namespaceURI, name, maybeType->IsNull() ? nullAtom : type, ec);
    if (ec) {
        setDOMException(ec, isolate);
        return;
    }
    v8SetReturnValue(args, toV8Fast(element.release(), args, document));
}

} // namespace WebCore
