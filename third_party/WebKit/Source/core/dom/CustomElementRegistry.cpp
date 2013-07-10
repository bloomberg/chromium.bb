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
#include "core/dom/CustomElementCallbackDispatcher.h"
#include "core/dom/CustomElementDefinition.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentLifecycleObserver.h"
#include "core/dom/Element.h"
#include "core/html/HTMLElement.h"
#include "core/svg/SVGElement.h"
#include "wtf/Vector.h"

namespace WebCore {

void setTypeExtension(Element* element, const AtomicString& typeExtension)
{
    ASSERT(element);
    if (!typeExtension.isEmpty())
        element->setAttribute(HTMLNames::isAttr, typeExtension);
}

static inline bool nameIncludesHyphen(const AtomicString& name)
{
    size_t hyphenPosition = name.find('-');
    return (hyphenPosition != notFound);
}

bool CustomElementRegistry::isValidTypeName(const AtomicString& name)
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

CustomElementDescriptor CustomElementRegistry::describe(Element* element) const
{
    ASSERT(element->isCustomElement());

    // If an element has a custom tag name it takes precedence over
    // the "is" attribute (if any).
    const AtomicString& type = isCustomTagName(element->localName())
        ? element->localName()
        : m_elementTypeMap.get(element);

    ASSERT(!type.isNull()); // Element must be in this registry
    return CustomElementDescriptor(type, element->namespaceURI(), element->localName());
}

class RegistrationContextObserver : public DocumentLifecycleObserver {
public:
    explicit RegistrationContextObserver(Document* document)
        : DocumentLifecycleObserver(document)
        , m_wentAway(!document)
    {
    }

    bool registrationContextWentAway() { return m_wentAway; }

private:
    virtual void documentWasDisposed() OVERRIDE { m_wentAway = true; }

    bool m_wentAway;
};

void CustomElementRegistry::registerElement(Document* document, CustomElementConstructorBuilder* constructorBuilder, const AtomicString& userSuppliedName, ExceptionCode& ec)
{
    RefPtr<CustomElementRegistry> protect(this);

    // FIXME: In every instance except one it is the
    // CustomElementConstructorBuilder that observes document
    // destruction during registration. This responsibility should be
    // consolidated in one place.
    RegistrationContextObserver observer(document);

    if (!constructorBuilder->isFeatureAllowed())
        return;

    AtomicString type = userSuppliedName.lower();
    if (!isValidTypeName(type)) {
        ec = InvalidCharacterError;
        return;
    }

    if (!constructorBuilder->validateOptions()) {
        ec = InvalidStateError;
        return;
    }

    QualifiedName tagName = nullQName();
    if (!constructorBuilder->findTagName(type, tagName)) {
        ec = NamespaceError;
        return;
    }
    ASSERT(tagName.namespaceURI() == HTMLNames::xhtmlNamespaceURI || tagName.namespaceURI() == SVGNames::svgNamespaceURI);

    if (m_registeredTypeNames.contains(type)) {
        ec = InvalidStateError;
        return;
    }

    ASSERT(!observer.registrationContextWentAway());

    RefPtr<CustomElementLifecycleCallbacks> lifecycleCallbacks = constructorBuilder->createCallbacks(document);

    // Consulting the constructor builder could execute script and
    // kill the document.
    if (observer.registrationContextWentAway()) {
        ec = InvalidStateError;
        return;
    }

    const CustomElementDescriptor descriptor(type, tagName.namespaceURI(), tagName.localName());
    RefPtr<CustomElementDefinition> definition = CustomElementDefinition::create(descriptor, lifecycleCallbacks);

    if (!constructorBuilder->createConstructor(document, definition.get())) {
        ec = NotSupportedError;
        return;
    }

    m_definitions.add(descriptor, definition);
    m_registeredTypeNames.add(descriptor.type());

    if (!constructorBuilder->didRegisterDefinition(definition.get())) {
        ec = NotSupportedError;
        return;
    }

    // Upgrade elements that were waiting for this definition.
    const CustomElementUpgradeCandidateMap::ElementSet& upgradeCandidates = m_candidates.takeUpgradeCandidatesFor(descriptor);
    for (CustomElementUpgradeCandidateMap::ElementSet::const_iterator it = upgradeCandidates.begin(); it != upgradeCandidates.end(); ++it)
        didResolveElement(definition.get(), *it);
}

CustomElementDefinition* CustomElementRegistry::findFor(Element* element) const
{
    ASSERT(element->document()->registry() == this);

    const CustomElementDescriptor& descriptor = describe(element);
    return find(descriptor);
}

CustomElementDefinition* CustomElementRegistry::find(const CustomElementDescriptor& descriptor) const
{
    return m_definitions.get(descriptor);
}

PassRefPtr<Element> CustomElementRegistry::createCustomTagElement(Document* document, const QualifiedName& tagName)
{
    if (!document)
        return 0;

    ASSERT(isCustomTagName(tagName.localName()));

    RefPtr<Element> element;

    if (HTMLNames::xhtmlNamespaceURI == tagName.namespaceURI())
        element = HTMLElement::create(tagName, document);
    else if (SVGNames::svgNamespaceURI == tagName.namespaceURI())
        element = SVGElement::create(tagName, document);
    else
        return Element::create(tagName, document);

    element->setIsCustomElement();

    const CustomElementDescriptor& descriptor = describe(element.get());
    CustomElementDefinition* definition = find(descriptor);
    if (definition)
        didResolveElement(definition, element.get());
    else
        didCreateUnresolvedElement(descriptor, element.get());

    return element.release();
}

void CustomElementRegistry::didGiveTypeExtension(Element* element, const AtomicString& type)
{
    if (!element->isHTMLElement() && !element->isSVGElement())
        return;
    if (element->isCustomElement())
        return; // A custom tag, which takes precedence over type extensions
    element->setIsCustomElement();
    m_elementTypeMap.add(element, type);
    const CustomElementDescriptor& descriptor = describe(element);
    CustomElementDefinition* definition = find(descriptor);
    if (definition)
        didResolveElement(definition, element);
    else
        didCreateUnresolvedElement(descriptor, element);
}

void CustomElementRegistry::didResolveElement(CustomElementDefinition* definition, Element* element) const
{
    CustomElementCallbackDispatcher::instance().enqueueCreatedCallback(definition->callbacks(), element);
}

void CustomElementRegistry::didCreateUnresolvedElement(const CustomElementDescriptor& descriptor, Element* element)
{
    m_candidates.add(descriptor, element);
}

void CustomElementRegistry::customElementWasDestroyed(Element* element)
{
    ASSERT(element->isCustomElement());
    m_candidates.remove(element);
    m_elementTypeMap.remove(element);
}

}
