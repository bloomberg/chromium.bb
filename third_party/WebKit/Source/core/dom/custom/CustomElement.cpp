// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/custom/CustomElement.h"

#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/custom/V0CustomElement.h"
#include "core/dom/custom/V0CustomElementRegistrationContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/html/HTMLElement.h"
#include "platform/text/Character.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

CustomElementsRegistry* CustomElement::registry(const Element& element)
{
    if (LocalDOMWindow* window = element.document().domWindow())
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
        QualifiedName(nullAtom, document.convertLocalName(localName), HTMLNames::xhtmlNamespaceURI),
        flags);
}

HTMLElement* CustomElement::createCustomElement(Document& document, const QualifiedName& tagName, CreateElementFlags flags)
{
    DCHECK(shouldCreateCustomElement(document, tagName));

    // TODO(kojii): Look up already defined custom elements when custom element
    // queues and upgrade are implemented.

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

} // namespace blink
