// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentAllNameCollection_h
#define DocumentAllNameCollection_h

#include "core/html/HTMLNameCollection.h"

namespace blink {

// DocumentAllNameCollection implements document.all namedItem as
// HTMLCollection.
class DocumentAllNameCollection final : public HTMLNameCollection {
 public:
  static DocumentAllNameCollection* Create(ContainerNode& document,
                                           CollectionType type,
                                           const AtomicString& name) {
    DCHECK_EQ(type, kDocumentAllNamedItems);
    return new DocumentAllNameCollection(document, name);
  }

  bool ElementMatches(const Element&) const;

 private:
  DocumentAllNameCollection(ContainerNode& document, const AtomicString& name);
};

DEFINE_TYPE_CASTS(DocumentAllNameCollection,
                  LiveNodeListBase,
                  collection,
                  collection->GetType() == kDocumentAllNamedItems,
                  collection.GetType() == kDocumentAllNamedItems);

}  // namespace blink

#endif  // DocumentAllNameCollection_h
