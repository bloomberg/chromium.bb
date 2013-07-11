/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "core/dom/CustomElementRegistrationContext.h"

#include "HTMLNames.h"
#include "SVGNames.h"
#include "core/dom/CustomElementCallbackDispatcher.h"
#include "core/dom/CustomElementDefinition.h"
#include "core/dom/CustomElementRegistry.h"
#include "core/dom/CustomElementUpgradeCandidateMap.h"
#include "core/dom/Element.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLUnknownElement.h"
#include "core/svg/SVGElement.h"

namespace WebCore {

// The null registration context is used by documents that do not
// process custom elements. All of its operations are no-ops.
class NullRegistrationContext : public CustomElementRegistrationContext {
public:
    NullRegistrationContext() { }
    virtual ~NullRegistrationContext() { }

    virtual CustomElementDescriptor describe(Element*) const OVERRIDE { return CustomElementDescriptor(); }

    virtual void registerElement(Document*, CustomElementConstructorBuilder*, const AtomicString&, ExceptionCode& ec) OVERRIDE { ec = NotSupportedError; }

    virtual PassRefPtr<Element> createCustomTagElement(Document*, const QualifiedName&) OVERRIDE;
    virtual void didGiveTypeExtension(Element*, const AtomicString&) OVERRIDE { }

    virtual void customElementAttributeDidChange(Element*, const AtomicString&, const AtomicString&, const AtomicString&) OVERRIDE { }
    virtual void customElementDidEnterDocument(Element*) OVERRIDE { }
    virtual void customElementDidLeaveDocument(Element*) OVERRIDE { }
    virtual void customElementIsBeingDestroyed(Element*) OVERRIDE { }
};

PassRefPtr<Element> NullRegistrationContext::createCustomTagElement(Document* document, const QualifiedName& tagName)
{
    ASSERT(isCustomTagName(tagName.localName()));

    if (!document)
        return 0;

    if (HTMLNames::xhtmlNamespaceURI == tagName.namespaceURI())
        return HTMLUnknownElement::create(tagName, document);
    if (SVGNames::svgNamespaceURI == tagName.namespaceURI())
        return SVGElement::create(tagName, document);
    return Element::create(tagName, document);
}

PassRefPtr<CustomElementRegistrationContext> CustomElementRegistrationContext::nullRegistrationContext()
{
    DEFINE_STATIC_LOCAL(RefPtr<NullRegistrationContext>, instance, ());
    if (!instance)
        instance = adoptRef(new NullRegistrationContext());
    return instance.get();
}

// An active registration context is used by documents that are
// processing custom elements.
class ActiveRegistrationContext : public CustomElementRegistrationContext {
public:
    ActiveRegistrationContext() { }
    virtual ~ActiveRegistrationContext() { }

    virtual CustomElementDescriptor describe(Element*) const OVERRIDE;

    virtual void registerElement(Document*, CustomElementConstructorBuilder*, const AtomicString& type, ExceptionCode&) OVERRIDE;

    virtual PassRefPtr<Element> createCustomTagElement(Document*, const QualifiedName&) OVERRIDE;
    virtual void didGiveTypeExtension(Element*, const AtomicString&) OVERRIDE;

    virtual void customElementAttributeDidChange(Element*, const AtomicString&, const AtomicString&, const AtomicString&) OVERRIDE;
    virtual void customElementDidEnterDocument(Element*) OVERRIDE;
    virtual void customElementDidLeaveDocument(Element*) OVERRIDE;
    virtual void customElementIsBeingDestroyed(Element*) OVERRIDE;

private:
    CustomElementDefinition* definitionFor(Element*) const;

    void didCreateUnresolvedElement(const CustomElementDescriptor&, Element*);
    void didResolveElement(CustomElementDefinition*, Element*) const;

    CustomElementRegistry m_registry;

    // Element creation
    typedef HashMap<Element*, AtomicString> ElementTypeMap;
    ElementTypeMap m_elementTypeMap; // Creation-time "is" attribute value.
    CustomElementUpgradeCandidateMap m_candidates;
};

CustomElementDescriptor ActiveRegistrationContext::describe(Element* element) const
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

void ActiveRegistrationContext::registerElement(Document* document, CustomElementConstructorBuilder* constructorBuilder, const AtomicString& type, ExceptionCode& ec)
{
    CustomElementDefinition* definition = m_registry.registerElement(document, constructorBuilder, type, ec);

    if (!definition)
        return;

    // Upgrade elements that were waiting for this definition.
    const CustomElementUpgradeCandidateMap::ElementSet& upgradeCandidates = m_candidates.takeUpgradeCandidatesFor(definition->descriptor());
    for (CustomElementUpgradeCandidateMap::ElementSet::const_iterator it = upgradeCandidates.begin(); it != upgradeCandidates.end(); ++it)
        didResolveElement(definition, *it);
}

CustomElementDefinition* ActiveRegistrationContext::definitionFor(Element* element) const
{
    ASSERT(element->document()->registrationContext() == this);
    const CustomElementDescriptor& descriptor = describe(element);
    return m_registry.find(descriptor);
}

PassRefPtr<Element> ActiveRegistrationContext::createCustomTagElement(Document* document, const QualifiedName& tagName)
{
    ASSERT(isCustomTagName(tagName.localName()));

    if (!document)
        return 0;

    RefPtr<Element> element;

    if (HTMLNames::xhtmlNamespaceURI == tagName.namespaceURI()) {
        element = HTMLElement::create(tagName, document);
    } else if (SVGNames::svgNamespaceURI == tagName.namespaceURI()) {
        element = SVGElement::create(tagName, document);
    } else {
        // XML elements are not custom elements, so return early.
        return Element::create(tagName, document);
    }

    element->setIsCustomElement();

    const CustomElementDescriptor& descriptor = describe(element.get());
    CustomElementDefinition* definition = m_registry.find(descriptor);
    if (definition)
        didResolveElement(definition, element.get());
    else
        didCreateUnresolvedElement(descriptor, element.get());

    return element.release();
}

void ActiveRegistrationContext::didCreateUnresolvedElement(const CustomElementDescriptor& descriptor, Element* element)
{
    m_candidates.add(descriptor, element);
}

void ActiveRegistrationContext::didResolveElement(CustomElementDefinition* definition, Element* element) const
{
    CustomElementCallbackDispatcher::instance().enqueueCreatedCallback(definition->callbacks(), element);
}

void ActiveRegistrationContext::didGiveTypeExtension(Element* element, const AtomicString& type)
{
    if (!element->isHTMLElement() && !element->isSVGElement())
        return;
    if (element->isCustomElement())
        return; // A custom tag, which takes precedence over type extensions
    element->setIsCustomElement();
    m_elementTypeMap.add(element, type);
    const CustomElementDescriptor& descriptor = describe(element);
    CustomElementDefinition* definition = m_registry.find(descriptor);
    if (definition)
        didResolveElement(definition, element);
    else
        didCreateUnresolvedElement(descriptor, element);
}

void ActiveRegistrationContext::customElementAttributeDidChange(Element* element, const AtomicString& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    ASSERT(element->isUpgradedCustomElement());
    CustomElementDefinition* definition = definitionFor(element);
    CustomElementCallbackDispatcher::instance().enqueueAttributeChangedCallback(definition->callbacks(), element, name, oldValue, newValue);
}

void ActiveRegistrationContext::customElementDidEnterDocument(Element* element)
{
    ASSERT(element->isUpgradedCustomElement());
    CustomElementDefinition* definition = definitionFor(element);
    CustomElementCallbackDispatcher::instance().enqueueEnteredDocumentCallback(definition->callbacks(), element);
}

void ActiveRegistrationContext::customElementDidLeaveDocument(Element* element)
{
    ASSERT(element->isUpgradedCustomElement());
    CustomElementDefinition* definition = definitionFor(element);
    CustomElementCallbackDispatcher::instance().enqueueLeftDocumentCallback(definition->callbacks(), element);
}

void ActiveRegistrationContext::customElementIsBeingDestroyed(Element* element)
{
    ASSERT(element->isCustomElement());
    m_candidates.remove(element);
    m_elementTypeMap.remove(element);
}

PassRefPtr<CustomElementRegistrationContext> CustomElementRegistrationContext::create()
{
    return adoptRef(new ActiveRegistrationContext());
}

bool CustomElementRegistrationContext::isValidTypeName(const AtomicString& name)
{
    if (notFound == name.find('-'))
        return false;

    // FIXME: Add annotation-xml.
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

bool CustomElementRegistrationContext::isCustomTagName(const AtomicString& localName)
{
    return isValidTypeName(localName);
}

void CustomElementRegistrationContext::setTypeExtension(Element* element, const AtomicString& type)
{
    ASSERT(element);
    if (!type.isEmpty())
        element->setAttribute(HTMLNames::isAttr, type);

    didGiveTypeExtension(element, type);
}

} // namespace WebCore
