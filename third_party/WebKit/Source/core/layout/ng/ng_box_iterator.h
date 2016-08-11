// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBoxIterator_h
#define NGBoxIterator_h

#include "core/layout/LayoutObject.h"

namespace blink {

class NGBox;

// NG Box iterator over siblings of the layout object.
class CORE_EXPORT NGBoxIterator
    : public std::iterator<std::forward_iterator_tag, NGBox> {
 public:
  class iterator {
   public:
    iterator& operator=(const iterator& otherValue);
    iterator(LayoutObject* layoutObject) : m_layoutObject(layoutObject) {}
    iterator& operator++();
    bool operator!=(const iterator& other);
    NGBox operator*();

   private:
    LayoutObject* m_layoutObject;
  };
  explicit NGBoxIterator(LayoutObject*);

  iterator begin() const;
  iterator end() const { return iterator(nullptr); }

 private:
  LayoutObject* m_layoutObject;
};

}  // namespace blink
#endif  // NGBoxIterator_h
