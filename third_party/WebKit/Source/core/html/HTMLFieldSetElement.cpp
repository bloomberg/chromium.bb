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

#include "core/html/HTMLFieldSetElement.h"

#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/NodeListsNodeData.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLLegendElement.h"
#include "core/layout/LayoutFieldset.h"
#include "platform/EventDispatchForbiddenScope.h"
#include "wtf/StdLibExtras.h"

namespace blink {

using namespace HTMLNames;

inline HTMLFieldSetElement::HTMLFieldSetElement(Document& document)
    : HTMLFormControlElement(fieldsetTag, document) {}

HTMLFieldSetElement* HTMLFieldSetElement::create(Document& document) {
  return new HTMLFieldSetElement(document);
}

bool HTMLFieldSetElement::matchesValidityPseudoClasses() const {
  return true;
}

bool HTMLFieldSetElement::isValidElement() {
  for (Element* element : *elements()) {
    if (element->isFormControlElement()) {
      if (!toHTMLFormControlElement(element)->checkValidity(
              nullptr, CheckValidityDispatchNoEvent))
        return false;
    }
  }
  return true;
}

bool HTMLFieldSetElement::isSubmittableElement() {
  return false;
}

// Returns a disabled focused element if it's in descendants of |base|.
Element*
HTMLFieldSetElement::invalidateDescendantDisabledStateAndFindFocusedOne(
    Element& base) {
  Element* focusedElement = adjustedFocusedElementInTreeScope();
  bool shouldBlur = false;
  {
    EventDispatchForbiddenScope eventForbidden;
    for (HTMLFormControlElement& element :
         Traversal<HTMLFormControlElement>::descendantsOf(base)) {
      element.ancestorDisabledStateWasChanged();
      if (focusedElement == &element && element.isDisabledFormControl())
        shouldBlur = true;
    }
  }
  return shouldBlur ? focusedElement : nullptr;
}

void HTMLFieldSetElement::disabledAttributeChanged() {
  // This element must be updated before the style of nodes in its subtree gets
  // recalculated.
  HTMLFormControlElement::disabledAttributeChanged();
  if (Element* focusedElement =
          invalidateDescendantDisabledStateAndFindFocusedOne(*this))
    focusedElement->blur();
}

void HTMLFieldSetElement::childrenChanged(const ChildrenChange& change) {
  HTMLFormControlElement::childrenChanged(change);
  Element* focusedElement = nullptr;
  {
    EventDispatchForbiddenScope eventForbidden;
    for (HTMLLegendElement& legend :
         Traversal<HTMLLegendElement>::childrenOf(*this)) {
      if (Element* element =
              invalidateDescendantDisabledStateAndFindFocusedOne(legend))
        focusedElement = element;
    }
  }
  if (focusedElement)
    focusedElement->blur();
}

bool HTMLFieldSetElement::supportsFocus() const {
  return HTMLElement::supportsFocus() && !isDisabledFormControl();
}

const AtomicString& HTMLFieldSetElement::formControlType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, fieldset, ("fieldset"));
  return fieldset;
}

LayoutObject* HTMLFieldSetElement::createLayoutObject(const ComputedStyle&) {
  return new LayoutFieldset(this);
}

HTMLLegendElement* HTMLFieldSetElement::legend() const {
  return Traversal<HTMLLegendElement>::firstChild(*this);
}

HTMLCollection* HTMLFieldSetElement::elements() {
  return ensureCachedCollection<HTMLCollection>(FormControls);
}

int HTMLFieldSetElement::tabIndex() const {
  return HTMLElement::tabIndex();
}

}  // namespace blink
