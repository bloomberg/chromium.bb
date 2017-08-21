// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/DocumentNameCollection.h"

#include "core/html/HTMLEmbedElement.h"
#include "core/html/HTMLObjectElement.h"

namespace blink {

DocumentNameCollection::DocumentNameCollection(ContainerNode& document,
                                               const AtomicString& name)
    : HTMLNameCollection(document, kDocumentNamedItems, name) {}

// https://html.spec.whatwg.org/multipage/dom.html#dom-document-nameditem-filter
bool DocumentNameCollection::ElementMatches(const HTMLElement& element) const {
  // Match images, forms, embeds, objects and iframes by name,
  // object by id, and images by id but only if they have
  // a name attribute (this very strange rule matches IE)
  if (isHTMLFormElement(element) || isHTMLIFrameElement(element) ||
      (isHTMLEmbedElement(element) && toHTMLEmbedElement(element).IsExposed()))
    return element.GetNameAttribute() == name_;
  if (isHTMLObjectElement(element) && toHTMLObjectElement(element).IsExposed())
    return element.GetNameAttribute() == name_ ||
           element.GetIdAttribute() == name_;
  if (isHTMLImageElement(element)) {
    const AtomicString& name_value = element.GetNameAttribute();
    return name_value == name_ ||
           (element.GetIdAttribute() == name_ && !name_value.IsEmpty());
  }
  return false;
}

}  // namespace blink
