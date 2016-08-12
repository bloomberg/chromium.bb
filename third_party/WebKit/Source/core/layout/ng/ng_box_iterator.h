// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGBoxIterator_h
#define NGBoxIterator_h

#include "core/layout/LayoutObject.h"
#include "core/layout/ng/ng_box.h"

namespace blink {

class NGBox;

// NG Box iterator over siblings of the layout object.
class CORE_EXPORT NGBoxIterator
    : public std::iterator<std::forward_iterator_tag, NGBox> {
 public:
  class iterator {
   public:
    iterator& operator=(const iterator& otherValue);
    iterator(NGBox box) : box_(box) {}
    iterator& operator++();
    bool operator!=(const iterator& other);
    NGBox operator*();

   private:
    NGBox box_;
  };
  explicit NGBoxIterator(NGBox);

  iterator begin() const;
  iterator end() const { return iterator(NGBox()); }

 private:
  NGBox box_;
};

}  // namespace blink
#endif  // NGBoxIterator_h
