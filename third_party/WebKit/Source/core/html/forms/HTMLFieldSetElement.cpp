/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "core/html/forms/HTMLFieldSetElement.h"

#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeListsNodeData.h"
#include "core/html/HTMLCollection.h"
#include "core/html/forms/HTMLLegendElement.h"
#include "core/html_names.h"
#include "core/layout/LayoutFieldset.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "platform/wtf/StdLibExtras.h"

namespace blink {

using namespace HTMLNames;

inline HTMLFieldSetElement::HTMLFieldSetElement(Document& document)
    : HTMLFormControlElement(fieldsetTag, document) {}

HTMLFieldSetElement* HTMLFieldSetElement::Create(Document& document) {
  return new HTMLFieldSetElement(document);
}

bool HTMLFieldSetElement::MatchesValidityPseudoClasses() const {
  return true;
}

bool HTMLFieldSetElement::IsValidElement() {
  for (Element* element : *elements()) {
    if (element->IsFormControlElement()) {
      if (!ToHTMLFormControlElement(element)->checkValidity(
              nullptr, kCheckValidityDispatchNoEvent))
        return false;
    }
  }
  return true;
}

bool HTMLFieldSetElement::IsSubmittableElement() {
  return false;
}

// Returns a disabled focused element if it's in descendants of |base|.
Element*
HTMLFieldSetElement::InvalidateDescendantDisabledStateAndFindFocusedOne(
    Element& base) {
  Element* focused_element = AdjustedFocusedElementInTreeScope();
  bool should_blur = false;
  {
    EventDispatchForbiddenScope event_forbidden;
    for (HTMLFormControlElement& element :
         Traversal<HTMLFormControlElement>::DescendantsOf(base)) {
      element.AncestorDisabledStateWasChanged();
      if (focused_element == &element && element.IsDisabledFormControl())
        should_blur = true;
    }
  }
  return should_blur ? focused_element : nullptr;
}

void HTMLFieldSetElement::DisabledAttributeChanged() {
  // This element must be updated before the style of nodes in its subtree gets
  // recalculated.
  HTMLFormControlElement::DisabledAttributeChanged();
  if (Element* focused_element =
          InvalidateDescendantDisabledStateAndFindFocusedOne(*this))
    focused_element->blur();
}

void HTMLFieldSetElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLFormControlElement::ChildrenChanged(change);
  Element* focused_element = nullptr;
  {
    EventDispatchForbiddenScope event_forbidden;
    for (HTMLLegendElement& legend :
         Traversal<HTMLLegendElement>::ChildrenOf(*this)) {
      if (Element* element =
              InvalidateDescendantDisabledStateAndFindFocusedOne(legend))
        focused_element = element;
    }
  }
  if (focused_element)
    focused_element->blur();
}

bool HTMLFieldSetElement::SupportsFocus() const {
  return HTMLElement::SupportsFocus() && !IsDisabledFormControl();
}

const AtomicString& HTMLFieldSetElement::FormControlType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, fieldset, ("fieldset"));
  return fieldset;
}

LayoutObject* HTMLFieldSetElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutFieldset(this);
}

HTMLLegendElement* HTMLFieldSetElement::Legend() const {
  return Traversal<HTMLLegendElement>::FirstChild(*this);
}

HTMLCollection* HTMLFieldSetElement::elements() {
  return EnsureCachedCollection<HTMLCollection>(kFormControls);
}

int HTMLFieldSetElement::tabIndex() const {
  return HTMLElement::tabIndex();
}

}  // namespace blink
