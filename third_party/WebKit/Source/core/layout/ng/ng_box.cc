// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_box.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box_iterator.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/LayoutBox.h"

namespace blink {

NGFragment* NGBox::layout(const NGConstraintSpace& constraintSpace) {
  NGBlockLayoutAlgorithm algorithm(style(), childIterator());
  m_layoutBox->clearNeedsLayout();
  NGFragment* fragment = algorithm.layout(constraintSpace);
  m_layoutBox->setLogicalWidth(fragment->inlineSize());
  m_layoutBox->setLogicalHeight(fragment->blockSize());
  return fragment;
}

const ComputedStyle* NGBox::style() const {
  return m_layoutBox->style();
}

NGBoxIterator NGBox::childIterator() {
  return NGBoxIterator(firstChild());
}

NGBox NGBox::nextSibling() const {
  return m_layoutBox ? NGBox(m_layoutBox->nextSibling()) : NGBox();
}

NGBox NGBox::firstChild() const {
  return m_layoutBox ? NGBox(m_layoutBox->slowFirstChild()) : NGBox();
}
}  // namespace blink
