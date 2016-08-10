// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_box.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"

namespace blink {

NGFragment* NGBox::layout(const NGConstraintSpace& constraintSpace) {
  NGBlockLayoutAlgorithm algorithm(style(), firstChild());
  return algorithm.layout(constraintSpace);
}

const ComputedStyle* NGBox::style() const {
  return m_layoutObject->style();
}

const NGBox NGBox::firstChild() const {
  return NGBox(m_layoutObject->slowFirstChild());
}

const NGBox NGBox::nextSibling() const {
  return NGBox(m_layoutObject->nextSibling());
}

}  // namespace blink
