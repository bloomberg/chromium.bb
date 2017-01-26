/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2010, 2011, 2012 Apple Inc. All
 * rights reserved.
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

#include "core/html/HTMLFormControlsCollection.h"

#include "bindings/core/v8/RadioNodeListOrElement.h"
#include "core/HTMLNames.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLImageElement.h"
#include "wtf/HashSet.h"

namespace blink {

using namespace HTMLNames;

// Since the collections are to be "live", we have to do the
// calculation every time if anything has changed.

HTMLFormControlsCollection::HTMLFormControlsCollection(ContainerNode& ownerNode)
    : HTMLCollection(ownerNode, FormControls, OverridesItemAfter),
      m_cachedElement(nullptr),
      m_cachedElementOffsetInArray(0) {
  DCHECK(isHTMLFormElement(ownerNode));
}

HTMLFormControlsCollection* HTMLFormControlsCollection::create(
    ContainerNode& ownerNode,
    CollectionType type) {
  DCHECK_EQ(type, FormControls);
  return new HTMLFormControlsCollection(ownerNode);
}

HTMLFormControlsCollection::~HTMLFormControlsCollection() {}

const ListedElement::List& HTMLFormControlsCollection::listedElements() const {
  return toHTMLFormElement(ownerNode()).listedElements();
}

const HeapVector<Member<HTMLImageElement>>&
HTMLFormControlsCollection::formImageElements() const {
  return toHTMLFormElement(ownerNode()).imageElements();
}

static unsigned findListedElement(const ListedElement::List& listedElements,
                                  Element* element) {
  unsigned i = 0;
  for (; i < listedElements.size(); ++i) {
    ListedElement* listedElement = listedElements[i];
    if (listedElement->isEnumeratable() &&
        toHTMLElement(listedElement) == element)
      break;
  }
  return i;
}

HTMLElement* HTMLFormControlsCollection::virtualItemAfter(
    Element* previous) const {
  const ListedElement::List& listedElements = this->listedElements();
  unsigned offset;
  if (!previous)
    offset = 0;
  else if (m_cachedElement == previous)
    offset = m_cachedElementOffsetInArray + 1;
  else
    offset = findListedElement(listedElements, previous) + 1;

  for (unsigned i = offset; i < listedElements.size(); ++i) {
    ListedElement* listedElement = listedElements[i];
    if (listedElement->isEnumeratable()) {
      m_cachedElement = toHTMLElement(listedElement);
      m_cachedElementOffsetInArray = i;
      return m_cachedElement;
    }
  }
  return nullptr;
}

void HTMLFormControlsCollection::invalidateCache(Document* oldDocument) const {
  HTMLCollection::invalidateCache(oldDocument);
  m_cachedElement = nullptr;
  m_cachedElementOffsetInArray = 0;
}

static HTMLElement* firstNamedItem(const ListedElement::List& elementsArray,
                                   const QualifiedName& attrName,
                                   const String& name) {
  DCHECK(attrName == idAttr || attrName == nameAttr);

  for (const auto& listedElement : elementsArray) {
    HTMLElement* element = toHTMLElement(listedElement);
    if (listedElement->isEnumeratable() &&
        element->fastGetAttribute(attrName) == name)
      return element;
  }
  return nullptr;
}

HTMLElement* HTMLFormControlsCollection::namedItem(
    const AtomicString& name) const {
  // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
  // This method first searches for an object with a matching id
  // attribute. If a match is not found, the method then searches for an
  // object with a matching name attribute, but only on those elements
  // that are allowed a name attribute.
  if (HTMLElement* item = firstNamedItem(listedElements(), idAttr, name))
    return item;
  return firstNamedItem(listedElements(), nameAttr, name);
}

void HTMLFormControlsCollection::updateIdNameCache() const {
  if (hasValidIdNameCache())
    return;

  NamedItemCache* cache = NamedItemCache::create();
  HashSet<StringImpl*> foundInputElements;

  for (const auto& listedElement : listedElements()) {
    if (listedElement->isEnumeratable()) {
      HTMLElement* element = toHTMLElement(listedElement);
      const AtomicString& idAttrVal = element->getIdAttribute();
      const AtomicString& nameAttrVal = element->getNameAttribute();
      if (!idAttrVal.isEmpty()) {
        cache->addElementWithId(idAttrVal, element);
        foundInputElements.insert(idAttrVal.impl());
      }
      if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal) {
        cache->addElementWithName(nameAttrVal, element);
        foundInputElements.insert(nameAttrVal.impl());
      }
    }
  }

  // HTMLFormControlsCollection doesn't support named getter for IMG
  // elements. However we still need to handle IMG elements here because
  // HTMLFormElement named getter relies on this.
  for (const auto& element : formImageElements()) {
    const AtomicString& idAttrVal = element->getIdAttribute();
    const AtomicString& nameAttrVal = element->getNameAttribute();
    if (!idAttrVal.isEmpty() && !foundInputElements.contains(idAttrVal.impl()))
      cache->addElementWithId(idAttrVal, element);
    if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal &&
        !foundInputElements.contains(nameAttrVal.impl()))
      cache->addElementWithName(nameAttrVal, element);
  }

  // Set the named item cache last as traversing the tree may cause cache
  // invalidation.
  setNamedItemCache(cache);
}

void HTMLFormControlsCollection::namedGetter(
    const AtomicString& name,
    RadioNodeListOrElement& returnValue) {
  HeapVector<Member<Element>> namedItems;
  this->namedItems(name, namedItems);

  if (namedItems.isEmpty())
    return;

  if (namedItems.size() == 1) {
    if (!isHTMLImageElement(*namedItems[0]))
      returnValue.setElement(namedItems.at(0));
    return;
  }

  // This path never returns a RadioNodeList for <img> because
  // onlyMatchingImgElements flag is false by default.
  returnValue.setRadioNodeList(ownerNode().radioNodeList(name));
}

void HTMLFormControlsCollection::supportedPropertyNames(Vector<String>& names) {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#htmlformcontrolscollection-0:
  // The supported property names consist of the non-empty values of all the id
  // and name attributes of all the elements represented by the collection, in
  // tree order, ignoring later duplicates, with the id of an element preceding
  // its name if it contributes both, they differ from each other, and neither
  // is the duplicate of an earlier entry.
  HashSet<AtomicString> existingNames;
  unsigned length = this->length();
  for (unsigned i = 0; i < length; ++i) {
    HTMLElement* element = item(i);
    DCHECK(element);
    const AtomicString& idAttribute = element->getIdAttribute();
    if (!idAttribute.isEmpty()) {
      HashSet<AtomicString>::AddResult addResult =
          existingNames.insert(idAttribute);
      if (addResult.isNewEntry)
        names.push_back(idAttribute);
    }
    const AtomicString& nameAttribute = element->getNameAttribute();
    if (!nameAttribute.isEmpty()) {
      HashSet<AtomicString>::AddResult addResult =
          existingNames.insert(nameAttribute);
      if (addResult.isNewEntry)
        names.push_back(nameAttribute);
    }
  }
}

DEFINE_TRACE(HTMLFormControlsCollection) {
  visitor->trace(m_cachedElement);
  HTMLCollection::trace(visitor);
}

}  // namespace blink
