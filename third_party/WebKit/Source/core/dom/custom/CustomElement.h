// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElement_h
#define CustomElement_h

#include "core/CoreExport.h"
#include "core/HTMLNames.h"
#include "core/dom/Element.h"
#include "wtf/Allocator.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class Document;
class HTMLElement;
class QualifiedName;
class CustomElementRegistry;

class CORE_EXPORT CustomElement {
    STATIC_ONLY(CustomElement);
public:
    // Retrieves the CustomElementsRegistry for Element, if any. This
    // may be a different object for a given element over its lifetime
    // as it moves between documents.
    static CustomElementsRegistry* registry(const Element&);

    // Returns true if element could possibly match a custom element
    // descriptor *now*. See CustomElementDescriptor::matches for the
    // meaning of "match". Custom element processing which depends on
    // matching a descriptor, such as upgrade, can be skipped for
    // elements that fail this test.
    //
    // Although this result is currently constant for a given element,
    // when customized built-in elements are implemented the result
    // will depend on the value of the 'is' attribute. In addition,
    // these elements may stop matching descriptors after being
    // upgraded, so use Node::getCustomElementState to detect
    // customized elements.
    static bool descriptorMayMatch(const Element& element)
    {
        // TODO(dominicc): Broaden this check when customized built-in
        // elements are implemented.
        return isValidName(element.localName())
            && element.namespaceURI() == HTMLNames::xhtmlNamespaceURI;
    }

    static bool isValidName(const AtomicString& name);

    static bool shouldCreateCustomElement(Document&, const AtomicString& localName);
    static bool shouldCreateCustomElement(Document&, const QualifiedName&);

    static HTMLElement* createCustomElement(Document&, const AtomicString& localName, CreateElementFlags);
    static HTMLElement* createCustomElement(Document&, const QualifiedName&, CreateElementFlags);
};

} // namespace blink

#endif // CustomElement_h
