// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLDataListOptionsCollection_h
#define HTMLDataListOptionsCollection_h

#include "core/html/HTMLCollection.h"
#include "core/html/forms/HTMLOptionElement.h"

namespace blink {

class HTMLDataListOptionsCollection : public HTMLCollection {
 public:
  static HTMLDataListOptionsCollection* Create(ContainerNode& owner_node,
                                               CollectionType type) {
    DCHECK_EQ(type, kDataListOptions);
    return new HTMLDataListOptionsCollection(owner_node);
  }

  HTMLOptionElement* Item(unsigned offset) const {
    return ToHTMLOptionElement(HTMLCollection::item(offset));
  }

  bool ElementMatches(const HTMLElement&) const;

 private:
  explicit HTMLDataListOptionsCollection(ContainerNode& owner_node)
      : HTMLCollection(owner_node,
                       kDataListOptions,
                       kDoesNotOverrideItemAfter) {}
};

DEFINE_TYPE_CASTS(HTMLDataListOptionsCollection,
                  LiveNodeListBase,
                  collection,
                  collection->GetType() == kDataListOptions,
                  collection.GetType() == kDataListOptions);

inline bool HTMLDataListOptionsCollection::ElementMatches(
    const HTMLElement& element) const {
  return IsHTMLOptionElement(element);
}

}  // namespace blink

#endif  // HTMLDataListOptionsCollection_h
