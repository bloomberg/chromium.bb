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
#include "SVGNames.h"
#include "bindings/v8/CustomElementConstructorBuilder.h"
#include "bindings/v8/CustomElementHelpers.h"
#include "core/dom/CustomElementDefinition.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/html/HTMLElement.h"
#include "core/svg/SVGElement.h"

namespace WebCore {

CustomElementInvocation::CustomElementInvocation(PassRefPtr<CustomElementCallback> callback, PassRefPtr<Element> element)
    : m_callback(callback)
    , m_element(element)
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
    : ContextLifecycleObserver(document)
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
        reservedNames.append(SVGNames::color_profileTag.localName());
        reservedNames.append(SVGNames::font_faceTag.localName());
        reservedNames.append(SVGNames::font_face_srcTag.localName());
        reservedNames.append(SVGNames::font_face_uriTag.localName());
        reservedNames.append(SVGNames::font_face_formatTag.localName());
        reservedNames.append(SVGNames::font_face_nameTag.localName());
        reservedNames.append(SVGNames::missing_glyphTag.localName());
    }

    if (notFound != reservedNames.find(name))
        return false;

    return Document::isValidName(name.string());
}

void CustomElementRegistry::registerElement(CustomElementConstructorBuilder* constructorBuilder, const AtomicString& userSuppliedName, ExceptionCode& ec)
{
    RefPtr<CustomElementRegistry> protect(this);

    if (!constructorBuilder->isFeatureAllowed())
        return;

    AtomicString type = userSuppliedName.lower();
    if (!isValidName(type)) {
        ec = INVALID_CHARACTER_ERR;
        return;
    }

    if (!constructorBuilder->validateOptions()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    QualifiedName tagName = nullQName();
    if (!constructorBuilder->findTagName(type, tagName)) {
        ec = NAMESPACE_ERR;
        return;
    }
    ASSERT(tagName.namespaceURI() == HTMLNames::xhtmlNamespaceURI || tagName.namespaceURI() == SVGNames::svgNamespaceURI);

    if (m_definitions.contains(type)) {
        ec = INVALID_STATE_ERR;
        return;
    }

    RefPtr<CustomElementCallback> lifecycleCallbacks = constructorBuilder->createCallback(document());

    // Consulting the constructor builder could execute script and
    // kill the document.
    if (!document()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    RefPtr<CustomElementDefinition> definition = CustomElementDefinition::create(type, tagName.localName(), tagName.namespaceURI(), lifecycleCallbacks);

    if (!constructorBuilder->createConstructor(document(), definition.get())) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }

    m_definitions.add(definition->type(), definition);

    // Upgrade elements that were waiting for this definition.
    CustomElementUpgradeCandidateMap::ElementSet upgradeCandidates = m_candidates.takeUpgradeCandidatesFor(definition.get());
    constructorBuilder->didRegisterDefinition(definition.get(), upgradeCandidates);

    for (CustomElementUpgradeCandidateMap::ElementSet::iterator it = upgradeCandidates.begin(); it != upgradeCandidates.end(); ++it) {
        (*it)->setNeedsStyleRecalc(); // :unresolved has changed

        enqueueReadyCallback(lifecycleCallbacks.get(), *it);
    }
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
    else if (SVGNames::svgNamespaceURI == tagName.namespaceURI())
        element = SVGElement::create(tagName, document());
    else
        return Element::create(tagName, document());

    element->setIsCustomElement();

    RefPtr<CustomElementDefinition> definition = findAndCheckNamespace(tagName.localName(), tagName.namespaceURI());
    if (!definition || definition->isTypeExtension()) {
        // If a definition for a type extension was available, this
        // custom tag element will be unresolved in perpetuity.
        didCreateUnresolvedElement(CustomElementDefinition::CustomTag, tagName.localName(), element.get());
    } else {
        didCreateCustomTagElement(definition.get(), element.get());
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
        enqueueReadyCallback(definition->callback(), element);
    }
}

void CustomElementRegistry::didCreateCustomTagElement(CustomElementDefinition* definition, Element* element)
{
    enqueueReadyCallback(definition->callback(), element);
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

void CustomElementRegistry::enqueueReadyCallback(CustomElementCallback* callback, Element* element)
{
    if (!callback->hasReady())
        return;

    bool wasInactive = m_invocations.isEmpty();
    m_invocations.append(CustomElementInvocation(callback, element));
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

        for (Vector<CustomElementInvocation>::iterator it = invocations.begin(); it != invocations.end(); ++it)
            it->callback()->ready(it->element());
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
