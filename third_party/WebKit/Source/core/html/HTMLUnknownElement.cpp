// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/HTMLUnknownElement.h"

#include "core/frame/UseCounter.h"

namespace blink {

HTMLUnknownElement::HTMLUnknownElement(const QualifiedName& tag_name,
                                       Document& document)
    : HTMLElement(tag_name, document) {
  if (tag_name.LocalName() == "data")
    UseCounter::Count(document, UseCounter::kDataElement);
  else if (tag_name.LocalName() == "time")
    UseCounter::Count(document, UseCounter::kTimeElement);
  else if (tag_name.LocalName() == "menuitem")
    UseCounter::Count(document, UseCounter::kMenuItemElement);
}

void HTMLUnknownElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == HTMLNames::iconAttr && HasTagName(HTMLNames::menuitemTag))
    UseCounter::Count(GetDocument(), UseCounter::kMenuItemElementIconAttribute);
  HTMLElement::ParseAttribute(params);
}

}  // namespace blink
