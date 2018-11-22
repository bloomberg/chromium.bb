// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/custom/element_internals.h"

#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/html/forms/html_form_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"

namespace blink {

ElementInternals::ElementInternals(HTMLElement& target) : target_(target) {}

void ElementInternals::Trace(Visitor* visitor) {
  visitor->Trace(target_);
  ListedElement::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

HTMLFormElement* ElementInternals::form() const {
  return ListedElement::Form();
}

void ElementInternals::DidUpgrade() {
  ContainerNode* parent = Target().parentNode();
  if (!parent)
    return;
  InsertedInto(*parent);
  if (auto* owner_form = form()) {
    if (auto* lists = owner_form->NodeLists())
      lists->InvalidateCaches(nullptr);
  }
  for (ContainerNode* node = parent; node; node = node->parentNode()) {
    if (IsHTMLFieldSetElement(node)) {
      // TODO(tkent): Invalidate only HTMLFormControlsCollections.
      if (auto* lists = node->NodeLists())
        lists->InvalidateCaches(nullptr);
    }
  }
}

bool ElementInternals::IsFormControlElement() const {
  return false;
}

bool ElementInternals::IsElementInternals() const {
  return true;
}

bool ElementInternals::IsEnumeratable() const {
  return true;
}

}  // namespace blink
