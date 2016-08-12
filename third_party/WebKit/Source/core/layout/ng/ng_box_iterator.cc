// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_box_iterator.h"
#include "core/layout/ng/ng_box.h"

namespace blink {

NGBoxIterator::NGBoxIterator(NGBox box) : box_(box) {}

NGBoxIterator::iterator& NGBoxIterator::iterator::operator=(
    const NGBoxIterator::iterator& otherValue) {
  box_ = otherValue.box_;
  return (*this);
}

NGBoxIterator::iterator& NGBoxIterator::iterator::operator++() {
  box_ = box_.nextSibling();
  return (*this);
}

bool NGBoxIterator::iterator::operator!=(const iterator& other) {
  return box_ != other.box_;
}

NGBox NGBoxIterator::iterator::operator*() {
  return box_;
}

NGBoxIterator::iterator NGBoxIterator::begin() const {
  return iterator(box_);
}

}  // namespace blink
