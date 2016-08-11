// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_box_iterator.h"
#include "core/layout/ng/ng_box.h"

namespace blink {

NGBoxIterator::NGBoxIterator(const LayoutObject* layoutObject)
    : m_layoutObject(layoutObject), m_currentChild(nullptr) {}

NGBox NGBoxIterator::first() {
  reset();
  return next();
}

NGBox NGBoxIterator::next() {
  if (!m_currentChild) {
    m_currentChild = m_layoutObject->slowFirstChild();
  } else {
    m_currentChild = m_layoutObject->nextSibling();
  }
  return NGBox(m_currentChild);
}

void NGBoxIterator::reset() {
  m_currentChild = nullptr;
}

}  // namespace blink
