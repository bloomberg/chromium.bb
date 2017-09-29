// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/WindowNameCollection.h"

#include "core/html/HTMLImageElement.h"

namespace blink {

WindowNameCollection::WindowNameCollection(ContainerNode& document,
                                           const AtomicString& name)
    : HTMLNameCollection(document, kWindowNamedItems, name) {}

bool WindowNameCollection::ElementMatches(const Element& element) const {
  // Match only images, forms, embeds and objects by name,
  // but anything by id
  if (IsHTMLImageElement(element) || IsHTMLFormElement(element) ||
      IsHTMLEmbedElement(element) || IsHTMLObjectElement(element)) {
    if (element.GetNameAttribute() == name_)
      return true;
  }
  return element.GetIdAttribute() == name_;
}

}  // namespace blink
