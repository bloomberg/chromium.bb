// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WindowNameCollection_h
#define WindowNameCollection_h

#include "core/html/HTMLNameCollection.h"

namespace blink {

class WindowNameCollection final : public HTMLNameCollection {
 public:
  static WindowNameCollection* Create(ContainerNode& document,
                                      CollectionType type,
                                      const AtomicString& name) {
    DCHECK_EQ(type, kWindowNamedItems);
    return new WindowNameCollection(document, name);
  }

  bool ElementMatches(const Element&) const;

 private:
  WindowNameCollection(ContainerNode& document, const AtomicString& name);
};

DEFINE_TYPE_CASTS(WindowNameCollection,
                  LiveNodeListBase,
                  collection,
                  collection->GetType() == kWindowNamedItems,
                  collection.GetType() == kWindowNamedItems);

}  // namespace blink

#endif  // WindowNameCollection_h
