/*
 * Copyright (C) 2006, 2011, 2012 Apple Computer, Inc.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#include "core/html/HTMLOptionsCollection.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/HTMLElementOrLong.h"
#include "bindings/core/v8/HTMLOptionElementOrHTMLOptGroupElement.h"
#include "bindings/core/v8/NodeListOrElement.h"
#include "core/dom/StaticNodeList.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/HTMLSelectElement.h"

namespace blink {

HTMLOptionsCollection::HTMLOptionsCollection(ContainerNode& select)
    : HTMLCollection(select, kSelectOptions, kDoesNotOverrideItemAfter) {
  DCHECK(isHTMLSelectElement(select));
}

void HTMLOptionsCollection::SupportedPropertyNames(Vector<String>& names) {
  // As per
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#htmloptionscollection:
  // The supported property names consist of the non-empty values of all the id
  // and name attributes of all the elements represented by the collection, in
  // tree order, ignoring later duplicates, with the id of an element preceding
  // its name if it contributes both, they differ from each other, and neither
  // is the duplicate of an earlier entry.
  HashSet<AtomicString> existing_names;
  unsigned length = this->length();
  for (unsigned i = 0; i < length; ++i) {
    Element* element = item(i);
    DCHECK(element);
    const AtomicString& id_attribute = element->GetIdAttribute();
    if (!id_attribute.IsEmpty()) {
      HashSet<AtomicString>::AddResult add_result =
          existing_names.insert(id_attribute);
      if (add_result.is_new_entry)
        names.push_back(id_attribute);
    }
    const AtomicString& name_attribute = element->GetNameAttribute();
    if (!name_attribute.IsEmpty()) {
      HashSet<AtomicString>::AddResult add_result =
          existing_names.insert(name_attribute);
      if (add_result.is_new_entry)
        names.push_back(name_attribute);
    }
  }
}

HTMLOptionsCollection* HTMLOptionsCollection::Create(ContainerNode& select,
                                                     CollectionType) {
  return new HTMLOptionsCollection(select);
}

void HTMLOptionsCollection::add(
    const HTMLOptionElementOrHTMLOptGroupElement& element,
    const HTMLElementOrLong& before,
    ExceptionState& exception_state) {
  toHTMLSelectElement(ownerNode()).add(element, before, exception_state);
}

void HTMLOptionsCollection::remove(int index) {
  toHTMLSelectElement(ownerNode()).remove(index);
}

int HTMLOptionsCollection::selectedIndex() const {
  return toHTMLSelectElement(ownerNode()).selectedIndex();
}

void HTMLOptionsCollection::setSelectedIndex(int index) {
  toHTMLSelectElement(ownerNode()).setSelectedIndex(index);
}

void HTMLOptionsCollection::setLength(unsigned length,
                                      ExceptionState& exception_state) {
  toHTMLSelectElement(ownerNode()).setLength(length, exception_state);
}

void HTMLOptionsCollection::namedGetter(const AtomicString& name,
                                        NodeListOrElement& return_value) {
  HeapVector<Member<Element>> named_items;
  this->NamedItems(name, named_items);

  if (!named_items.size())
    return;

  if (named_items.size() == 1) {
    return_value.setElement(named_items.at(0));
    return;
  }

  // FIXME: The spec and Firefox do not return a NodeList. They always return
  // the first matching Element.
  UseCounter::Count(
      GetDocument(),
      WebFeature::kHTMLOptionsCollectionNamedGetterReturnsNodeList);
  return_value.setNodeList(StaticElementList::Adopt(named_items));
}

bool HTMLOptionsCollection::AnonymousIndexedSetter(
    unsigned index,
    HTMLOptionElement* value,
    ExceptionState& exception_state) {
  HTMLSelectElement& base = toHTMLSelectElement(ownerNode());
  if (!value) {  // undefined or null
    base.remove(index);
    return true;
  }
  base.SetOption(index, value, exception_state);
  return true;
}

}  // namespace blink
