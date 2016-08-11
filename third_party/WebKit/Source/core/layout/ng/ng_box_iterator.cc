// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_box_iterator.h"
#include "core/layout/ng/ng_box.h"

namespace blink {

NGBoxIterator::NGBoxIterator(LayoutObject* layoutObject)
    : m_layoutObject(layoutObject) {}

NGBoxIterator::iterator& NGBoxIterator::iterator::operator=(
    const NGBoxIterator::iterator& otherValue) {
  m_layoutObject = otherValue.m_layoutObject;
  return (*this);
}

NGBoxIterator::iterator& NGBoxIterator::iterator::operator++() {
  if (m_layoutObject)
    m_layoutObject = m_layoutObject->nextSibling();
  return (*this);
}

bool NGBoxIterator::iterator::operator!=(const iterator& other) {
  return m_layoutObject != other.m_layoutObject;
}

NGBox NGBoxIterator::iterator::operator*() {
  return NGBox(m_layoutObject);
}

NGBoxIterator::iterator NGBoxIterator::begin() const {
  if (m_layoutObject)
    return iterator(m_layoutObject->slowFirstChild());
  return iterator(nullptr);
}

}  // namespace blink
