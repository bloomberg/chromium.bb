// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElement.h"

#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/custom/CEReactionsScope.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "core/dom/custom/CustomElementReactionStack.h"
#include "core/dom/custom/CustomElementsRegistry.h"
#include "core/dom/custom/V0CustomElement.h"
#include "core/dom/custom/V0CustomElementRegistrationContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLUnknownElement.h"
#include "platform/text/Character.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

CustomElementsRegistry* CustomElement::registry(const Element& element)
{
    return registry(element.document());
}
CustomElementsRegistry* CustomElement::registry(const Document& document)
{
    if (LocalDOMWindow* window = document.domWindow())
        return window->customElements();
    return nullptr;
}

static CustomElementDefinition* definitionForElementWithoutCheck(const Element& element)
{
    DCHECK_EQ(element.getCustomElementState(), CustomElementState::Custom);
    return element.customElementDefinition();
}

CustomElementDefinition* CustomElement::definitionForElement(const Element* element)
{
    if (!element || element->getCustomElementState() != CustomElementState::Custom)
        return nullptr;
    return definitionForElementWithoutCheck(*element);
}

bool CustomElement::isValidName(const AtomicString& name)
{
    if (!name.length() || name[0] < 'a' || name[0] > 'z')
        return false;

    bool hasHyphens = false;
    for (size_t i = 1; i < name.length(); ) {
        UChar32 ch;
        if (name.is8Bit())
            ch = name[i++];
        else
            U16_NEXT(name.characters16(), i, name.length(), ch);
        if (ch == '-')
            hasHyphens = true;
        else if (!Character::isPotentialCustomElementNameChar(ch))
            return false;
    }
    if (!hasHyphens)
        return false;

    // https://html.spec.whatwg.org/multipage/scripting.html#valid-custom-element-name
    DEFINE_STATIC_LOCAL(HashSet<AtomicString>, hyphenContainingElementNames, ({
        "annotation-xml",
        "color-profile",
        "font-face",
        "font-face-src",
        "font-face-uri",
        "font-face-format",
        "font-face-name",
        "missing-glyph",
    }));
    return !hyphenContainingElementNames.contains(name);
}

bool CustomElement::shouldCreateCustomElement(const AtomicString& localName)
{
    return RuntimeEnabledFeatures::customElementsV1Enabled()
        && isValidName(localName);
}

bool CustomElement::shouldCreateCustomElement(const QualifiedName& tagName)
{
    return shouldCreateCustomElement(tagName.localName())
        && tagName.namespaceURI() == HTMLNames::xhtmlNamespaceURI;
}

static CustomElementDefinition* definitionForName(const Document& document, const QualifiedName& name)
{
    if (CustomElementsRegistry* registry = CustomElement::registry(document))
        return registry->definitionForName(name.localName());
    return nullptr;
}

HTMLElement* CustomElement::createCustomElementSync(Document& document, const AtomicString& localName, ExceptionState& exceptionState)
{
    return createCustomElementSync(document,
        QualifiedName(nullAtom, localName, HTMLNames::xhtmlNamespaceURI),
        exceptionState);
}

HTMLElement* CustomElement::createCustomElementSync(Document& document, const QualifiedName& tagName, ExceptionState& exceptionState)
{
    CHECK(shouldCreateCustomElement(tagName));

    // To create an element:
    // https://dom.spec.whatwg.org/#concept-create-element
    // 6. If definition is non-null, then:
    // 6.1. If the synchronous custom elements flag is set:
    if (CustomElementDefinition* definition = definitionForName(document, tagName))
        return definition->createElementSync(document, tagName, exceptionState);

    return createUndefinedElement(document, tagName);
}

HTMLElement* CustomElement::createCustomElementSync(Document& document, const QualifiedName& tagName)
{
    CHECK(shouldCreateCustomElement(tagName));

    // When invoked from "create an element for a token":
    // https://html.spec.whatwg.org/multipage/syntax.html#create-an-element-for-the-token
    // 7. If this step throws an exception, then report the exception,
    // and let element be instead a new element that implements
    // HTMLUnknownElement.
    if (CustomElementDefinition* definition = definitionForName(document, tagName))
        return definition->createElementSync(document, tagName);

    return createUndefinedElement(document, tagName);
}

HTMLElement* CustomElement::createCustomElementAsync(Document& document, const QualifiedName& tagName)
{
    CHECK(shouldCreateCustomElement(tagName));

    // To create an element:
    // https://dom.spec.whatwg.org/#concept-create-element
    // 6. If definition is non-null, then:
    // 6.2. If the synchronous custom elements flag is not set:
    if (CustomElementDefinition* definition = definitionForName(document, tagName))
        return definition->createElementAsync(document, tagName);

    return createUndefinedElement(document, tagName);
}

HTMLElement* CustomElement::createUndefinedElement(Document& document, const QualifiedName& tagName)
{
    DCHECK(shouldCreateCustomElement(tagName));

    HTMLElement* element;
    if (V0CustomElement::isValidName(tagName.localName()) && document.registrationContext()) {
        Element* v0element = document.registrationContext()->createCustomTagElement(document, tagName);
        SECURITY_DCHECK(v0element->isHTMLElement());
        element = toHTMLElement(v0element);
    } else {
        element = HTMLElement::create(tagName, document);
    }

    element->setCustomElementState(CustomElementState::Undefined);

    return element;
}

HTMLElement* CustomElement::createFailedElement(Document& document, const QualifiedName& tagName)
{
    DCHECK(shouldCreateCustomElement(tagName));

    // "create an element for a token":
    // https://html.spec.whatwg.org/multipage/syntax.html#create-an-element-for-the-token

    // 7. If this step throws an exception, let element be instead a new element
    // that implements HTMLUnknownElement, with no attributes, namespace set to
    // given namespace, namespace prefix set to null, custom element state set
    // to "failed", and node document set to document.

    HTMLElement* element = HTMLUnknownElement::create(tagName, document);
    element->setCustomElementState(CustomElementState::Failed);
    return element;
}

void CustomElement::enqueue(Element* element, CustomElementReaction* reaction)
{
    // To enqueue an element on the appropriate element queue
    // https://html.spec.whatwg.org/multipage/scripting.html#enqueue-an-element-on-the-appropriate-element-queue

    // If the custom element reactions stack is not empty, then
    // Add element to the current element queue.
    if (CEReactionsScope* current = CEReactionsScope::current()) {
        current->enqueueToCurrentQueue(element, reaction);
        return;
    }

    // If the custom element reactions stack is empty, then
    // Add element to the backup element queue.
    CustomElementReactionStack::current().enqueueToBackupQueue(element, reaction);
}

void CustomElement::enqueueConnectedCallback(Element* element)
{
    CustomElementDefinition* definition = definitionForElementWithoutCheck(*element);
    if (definition->hasConnectedCallback())
        definition->enqueueConnectedCallback(element);
}

void CustomElement::enqueueDisconnectedCallback(Element* element)
{
    CustomElementDefinition* definition = definitionForElementWithoutCheck(*element);
    if (definition->hasDisconnectedCallback())
        definition->enqueueDisconnectedCallback(element);
}

void CustomElement::enqueueAdoptedCallback(Element* element)
{
    DCHECK_EQ(element->getCustomElementState(), CustomElementState::Custom);
    CustomElementDefinition* definition = definitionForElementWithoutCheck(*element);
    if (definition->hasAdoptedCallback())
        definition->enqueueAdoptedCallback(element);
}

void CustomElement::enqueueAttributeChangedCallback(Element* element,
    const QualifiedName& name,
    const AtomicString& oldValue, const AtomicString& newValue)
{
    CustomElementDefinition* definition = definitionForElementWithoutCheck(*element);
    if (definition->hasAttributeChangedCallback(name))
        definition->enqueueAttributeChangedCallback(element, name, oldValue, newValue);
}

void CustomElement::tryToUpgrade(Element* element)
{
    // Try to upgrade an element
    // https://html.spec.whatwg.org/multipage/scripting.html#concept-try-upgrade

    DCHECK_EQ(element->getCustomElementState(), CustomElementState::Undefined);

    CustomElementsRegistry* registry = CustomElement::registry(*element);
    if (!registry)
        return;
    if (CustomElementDefinition* definition = registry->definitionForName(element->localName()))
        definition->enqueueUpgradeReaction(element);
    else
        registry->addCandidate(element);
}

} // namespace blink
