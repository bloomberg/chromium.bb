/*
 * Copyright (C) 2009, 2011, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/HTMLAllCollection.h"

#include "bindings/core/v8/node_list_or_element.h"
#include "core/dom/Element.h"
#include "core/dom/StaticNodeList.h"

namespace blink {

HTMLAllCollection* HTMLAllCollection::Create(ContainerNode& node,
                                             CollectionType type) {
  DCHECK_EQ(type, kDocAll);
  return new HTMLAllCollection(node);
}

HTMLAllCollection::HTMLAllCollection(ContainerNode& node)
    : HTMLCollection(node, kDocAll, kDoesNotOverrideItemAfter) {}

HTMLAllCollection::~HTMLAllCollection() {}

Element* HTMLAllCollection::NamedItemWithIndex(const AtomicString& name,
                                               unsigned index) const {
  UpdateIdNameCache();

  const NamedItemCache& cache = GetNamedItemCache();
  if (const auto* elements = cache.GetElementsById(name)) {
    if (index < elements->size())
      return elements->at(index);
    index -= elements->size();
  }

  if (const auto* elements = cache.GetElementsByName(name)) {
    if (index < elements->size())
      return elements->at(index);
  }

  return nullptr;
}

void HTMLAllCollection::namedGetter(const AtomicString& name,
                                    NodeListOrElement& return_value) {
  HeapVector<Member<Element>> named_items;
  this->NamedItems(name, named_items);

  if (!named_items.size())
    return;

  if (named_items.size() == 1) {
    return_value.SetElement(named_items.at(0));
    return;
  }

  // FIXME: HTML5 specification says this should be a HTMLCollection.
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-dom-interfaces.html#htmlallcollection
  return_value.SetNodeList(StaticElementList::Adopt(named_items));
}

}  // namespace blink
