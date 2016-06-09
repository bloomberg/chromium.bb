// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElement.h"

#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/custom/CEReactionsScope.h"
#include "core/dom/custom/CustomElementDefinition.h"
#include "core/dom/custom/CustomElementUpgradeReaction.h"
#include "core/dom/custom/CustomElementsRegistry.h"
#include "core/dom/custom/V0CustomElement.h"
#include "core/dom/custom/V0CustomElementRegistrationContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/html/HTMLElement.h"
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
    DEFINE_STATIC_LOCAL(HashSet<AtomicString>, hyphenContainingElementNames, ());
    if (hyphenContainingElementNames.isEmpty()) {
        hyphenContainingElementNames.add("annotation-xml");
        hyphenContainingElementNames.add("color-profile");
        hyphenContainingElementNames.add("font-face");
        hyphenContainingElementNames.add("font-face-src");
        hyphenContainingElementNames.add("font-face-uri");
        hyphenContainingElementNames.add("font-face-format");
        hyphenContainingElementNames.add("font-face-name");
        hyphenContainingElementNames.add("missing-glyph");
    }

    return !hyphenContainingElementNames.contains(name);
}

bool CustomElement::shouldCreateCustomElement(Document& document, const AtomicString& localName)
{
    return RuntimeEnabledFeatures::customElementsV1Enabled()
        && document.frame() && isValidName(localName);
}

bool CustomElement::shouldCreateCustomElement(Document& document, const QualifiedName& tagName)
{
    return shouldCreateCustomElement(document, tagName.localName())
        && tagName.namespaceURI() == HTMLNames::xhtmlNamespaceURI;
}

HTMLElement* CustomElement::createCustomElement(Document& document, const AtomicString& localName, CreateElementFlags flags)
{
    return createCustomElement(document,
        QualifiedName(nullAtom, localName, HTMLNames::xhtmlNamespaceURI),
        flags);
}

HTMLElement* CustomElement::createCustomElement(Document& document, const QualifiedName& tagName, CreateElementFlags flags)
{
    DCHECK(shouldCreateCustomElement(document, tagName));

    // To create an element:
    // https://dom.spec.whatwg.org/#concept-create-element
    // 6. If definition is non-null, then:
    HTMLElement* element;
    if (CustomElementsRegistry* registry = CustomElement::registry(document)) {
        if (CustomElementDefinition* definition = registry->definitionForName(tagName.localName())) {
            // 6.2. If the synchronous custom elements flag is not set:
            if (flags & AsynchronousCustomElements)
                return createCustomElementAsync(document, *definition, tagName);

            // TODO(kojii): Synchronous mode implementation coming after async.
        }
    }

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

HTMLElement* CustomElement::createCustomElementAsync(Document& document,
    CustomElementDefinition& definition, const QualifiedName& tagName)
{
    // https://dom.spec.whatwg.org/#concept-create-element
    // 6. If definition is non-null, then:
    // 6.2. If the synchronous custom elements flag is not set:
    // 6.2.1. Set result to a new element that implements the HTMLElement
    // interface, with no attributes, namespace set to the HTML namespace,
    // namespace prefix set to prefix, local name set to localName, custom
    // element state set to "undefined", and node document set to document.
    HTMLElement* element = HTMLElement::create(tagName, document);
    element->setCustomElementState(CustomElementState::Undefined);
    // 6.2.2. Enqueue a custom element upgrade reaction given result and
    // definition.
    enqueueUpgradeReaction(element, &definition);
    return element;
}

void CustomElement::enqueueUpgradeReaction(Element* element, CustomElementDefinition* definition)
{
    // CEReactionsScope must be created by [CEReactions] in IDL,
    // or callers must setup explicitly if it does not go through bindings.
    DCHECK(CEReactionsScope::current());
    CEReactionsScope::current()->enqueue(element,
        new CustomElementUpgradeReaction(definition));
}

} // namespace blink
