/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "core/dom/CustomElementRegistry.h"

#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/CustomElementHelpers.h"
#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ScriptValue.h"
#include "core/dom/CustomElementDefinition.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/html/HTMLElement.h"

#if ENABLE(SVG)
#include "SVGNames.h"
#include "core/svg/SVGElement.h"
#endif

namespace WebCore {

CustomElementInvocation::CustomElementInvocation(PassRefPtr<Element> element)
    : m_element(element)
{
}

CustomElementInvocation::~CustomElementInvocation()
{
}

void setTypeExtension(Element* element, const AtomicString& typeExtension)
{
    ASSERT(element);
    if (!typeExtension.isEmpty())
        element->setAttribute(HTMLNames::isAttr, typeExtension);
}

CustomElementRegistry::CustomElementRegistry(Document* document)
    : ContextDestructionObserver(document)
{
}

CustomElementRegistry::~CustomElementRegistry()
{
    deactivate();
}

static inline bool nameIncludesHyphen(const AtomicString& name)
{
    size_t hyphenPosition = name.find('-');
    return (hyphenPosition != notFound);
}

bool CustomElementRegistry::isValidName(const AtomicString& name)
{
    if (!nameIncludesHyphen(name))
        return false;

    DEFINE_STATIC_LOCAL(Vector<AtomicString>, reservedNames, ());
    if (reservedNames.isEmpty()) {
#if ENABLE(SVG)
        reservedNames.append(SVGNames::color_profileTag.localName());
        reservedNames.append(SVGNames::font_faceTag.localName());
        reservedNames.append(SVGNames::font_face_srcTag.localName());
        reservedNames.append(SVGNames::font_face_uriTag.localName());
        reservedNames.append(SVGNames::font_face_formatTag.localName());
        reservedNames.append(SVGNames::font_face_nameTag.localName());
        reservedNames.append(SVGNames::missing_glyphTag.localName());
#endif
    }

    if (notFound != reservedNames.find(name))
        return false;

    return Document::isValidName(name.string());
}

PassRefPtr<CustomElementConstructor> CustomElementRegistry::registerElement(ScriptState* state, const AtomicString& userSuppliedName, const Dictionary& options, ExceptionCode& ec)
{
    RefPtr<CustomElementRegistry> protect(this);

    if (!CustomElementHelpers::isFeatureAllowed(state))
        return 0;

    AtomicString name = userSuppliedName.lower();
    if (!isValidName(name)) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }

    ScriptValue prototypeValue;
    if (!options.get("prototype", prototypeValue)) {
        // FIXME: Implement the default value handling.
        // Currently default value of the "prototype" parameter, which
        // is HTMLSpanElement.prototype, has an ambiguity about its
        // behavior. The spec should be fixed before WebKit implements
        // it. https://www.w3.org/Bugs/Public/show_bug.cgi?id=20801
        ec = INVALID_STATE_ERR;
        return 0;
    }

    AtomicString namespaceURI;
    if (!CustomElementHelpers::isValidPrototypeParameter(prototypeValue, state, namespaceURI)) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    if (namespaceURI.isNull()) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    AtomicString type = name;
    if (m_definitions.contains(type)) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    const QualifiedName* prototypeTagName = CustomElementHelpers::findLocalName(prototypeValue);
    if (prototypeTagName)
        name = prototypeTagName->localName();

    // A script execution could happen in isValidPrototypeParameter(), which kills the document.
    if (!document()) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    RefPtr<CustomElementDefinition> definition = CustomElementDefinition::create(state, type, name, namespaceURI, prototypeValue);

    RefPtr<CustomElementConstructor> constructor = CustomElementConstructor::create(document(), definition->tagQName(), definition->isTypeExtension() ? definition->type() : nullAtom);
    if (!CustomElementHelpers::initializeConstructorWrapper(constructor.get(), prototypeValue, state)) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    m_definitions.add(definition->type(), definition);

    // Upgrade elements that were waiting for this definition.
    CustomElementUpgradeCandidateMap::ElementSet upgradeCandidates = m_candidates.takeUpgradeCandidatesFor(definition.get());
    CustomElementHelpers::upgradeWrappers(document(), upgradeCandidates, definition->prototype());
    for (CustomElementUpgradeCandidateMap::ElementSet::iterator it = upgradeCandidates.begin(); it != upgradeCandidates.end(); ++it) {
        (*it)->setNeedsStyleRecalc(); // :unresolved has changed
        activate(CustomElementInvocation(*it));
    }

    return constructor.release();
}

bool CustomElementRegistry::isUnresolved(Element* element) const
{
    return m_candidates.contains(element);
}

PassRefPtr<CustomElementDefinition> CustomElementRegistry::findFor(Element* element) const
{
    ASSERT(element->document()->registry() == this);

    if (!element->isCustomElement())
        return 0;

    // When a custom tag and a type extension are provided as element
    // names at the same time, the custom tag takes precedence.
    if (isCustomTagName(element->localName())) {
        if (RefPtr<CustomElementDefinition> definition = findAndCheckNamespace(element->localName(), element->namespaceURI()))
            return definition->isTypeExtension() ? 0 : definition.release();
    }

    // FIXME: Casually consulting the "is" attribute is dangerous if
    // it could have changed since element creation.
    const AtomicString& isValue = element->getAttribute(HTMLNames::isAttr);
    if (RefPtr<CustomElementDefinition> definition = findAndCheckNamespace(isValue, element->namespaceURI()))
        return definition->isTypeExtension() && definition->name() == element->localName() ? definition.release() : 0;

    return 0;
}

PassRefPtr<CustomElementDefinition> CustomElementRegistry::findAndCheckNamespace(const AtomicString& type, const AtomicString& namespaceURI) const
{
    if (type.isNull())
        return 0;
    DefinitionMap::const_iterator it = m_definitions.find(type);
    if (it == m_definitions.end())
        return 0;
    if (it->value->namespaceURI() != namespaceURI)
        return 0;
    return it->value;
}

PassRefPtr<Element> CustomElementRegistry::createCustomTagElement(const QualifiedName& tagName)
{
    if (!document())
        return 0;

    ASSERT(isCustomTagName(tagName.localName()));

    RefPtr<Element> element;

    if (HTMLNames::xhtmlNamespaceURI == tagName.namespaceURI())
        element = HTMLElement::create(tagName, document());
#if ENABLE(SVG)
    else if (SVGNames::svgNamespaceURI == tagName.namespaceURI())
        element = SVGElement::create(tagName, document());
#endif
    else
        return Element::create(tagName, document());

    element->setIsCustomElement();

    RefPtr<CustomElementDefinition> definition = findAndCheckNamespace(tagName.localName(), tagName.namespaceURI());
    if (!definition || definition->isTypeExtension()) {
        // If a definition for a type extension was available, this
        // custom tag element will be unresolved in perpetuity.
        didCreateUnresolvedElement(CustomElementDefinition::CustomTag, tagName.localName(), element.get());
    } else {
        didCreateCustomTagElement(element.get());
    }

    return element.release();
}

void CustomElementRegistry::didGiveTypeExtension(Element* element, const AtomicString& type)
{
    if (!element->isHTMLElement() && !element->isSVGElement())
        return;
    element->setIsCustomElement();
    RefPtr<CustomElementDefinition> definition = findFor(element);
    if (!definition || !definition->isTypeExtension()) {
        // If a definition for a custom tag was available, this type
        // extension element will be unresolved in perpetuity.
        didCreateUnresolvedElement(CustomElementDefinition::TypeExtension, type, element);
    } else {
        activate(CustomElementInvocation(element));
    }
}

void CustomElementRegistry::didCreateCustomTagElement(Element* element)
{
    activate(CustomElementInvocation(element));
}

void CustomElementRegistry::didCreateUnresolvedElement(CustomElementDefinition::CustomElementKind kind, const AtomicString& type, Element* element)
{
    m_candidates.add(kind, type, element);
}

void CustomElementRegistry::customElementWasDestroyed(Element* element)
{
    ASSERT(element->isCustomElement());
    m_candidates.remove(element);
}

void CustomElementRegistry::activate(const CustomElementInvocation& invocation)
{
    bool wasInactive = m_invocations.isEmpty();
    m_invocations.append(invocation);
    if (wasInactive)
        activeCustomElementRegistries().add(this);
}

void CustomElementRegistry::deactivate()
{
    ASSERT(m_invocations.isEmpty());
    if (activeCustomElementRegistries().contains(this))
        activeCustomElementRegistries().remove(this);
}

inline Document* CustomElementRegistry::document() const
{
    return toDocument(m_scriptExecutionContext);
}

void CustomElementRegistry::deliverLifecycleCallbacks()
{
    ASSERT(!m_invocations.isEmpty());

    if (!m_invocations.isEmpty()) {
        Vector<CustomElementInvocation> invocations;
        m_invocations.swap(invocations);
        CustomElementHelpers::invokeReadyCallbacksIfNeeded(m_scriptExecutionContext, invocations);
    }

    ASSERT(m_invocations.isEmpty());
    deactivate();
}

void CustomElementRegistry::deliverAllLifecycleCallbacks()
{
    while (!activeCustomElementRegistries().isEmpty()) {
        Vector<RefPtr<CustomElementRegistry> > registries;
        copyToVector(activeCustomElementRegistries(), registries);
        activeCustomElementRegistries().clear();
        for (size_t i = 0; i < registries.size(); ++i)
            registries[i]->deliverLifecycleCallbacks();
    }
}

}
