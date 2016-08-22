// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_box.h"

#include "core/layout/LayoutObject.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box_iterator.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutBox.h"

namespace blink {

NGFragment* NGBox::layout(const NGConstraintSpace& constraintSpace) {
  // We can either use the new layout code to do the layout and then copy the
  // resulting size to the LayoutObject, or use the old layout code and
  // synthesize a fragment.
  NGFragment* fragment = nullptr;
  if (canUseNewLayout()) {
    NGBlockLayoutAlgorithm algorithm(style(), childIterator());
    fragment = algorithm.layout(constraintSpace);
    m_layoutBox->setLogicalWidth(fragment->inlineSize());
    m_layoutBox->setLogicalHeight(fragment->blockSize());
    if (m_layoutBox->isLayoutBlock())
      toLayoutBlock(m_layoutBox)->layoutPositionedObjects(true);
    m_layoutBox->clearNeedsLayout();
  } else {
    // TODO(layout-ng): If fixedSize is true, set the override width/height too
    NGLogicalSize containerSize = constraintSpace.ContainerSize();
    m_layoutBox->setOverrideContainingBlockContentLogicalWidth(
        containerSize.inlineSize);
    m_layoutBox->setOverrideContainingBlockContentLogicalHeight(
        containerSize.blockSize);
    if (m_layoutBox->isLayoutNGBlockFlow() && m_layoutBox->needsLayout()) {
      toLayoutNGBlockFlow(m_layoutBox)->LayoutBlockFlow::layoutBlock(true);
    } else {
      m_layoutBox->layoutIfNeeded();
    }
    LayoutRect overflow = m_layoutBox->layoutOverflowRect();
    // TODO(layout-ng): This does not handle writing modes correctly (for
    // overflow & the enums)
    fragment = new NGFragment(
        m_layoutBox->logicalWidth(), m_layoutBox->logicalHeight(),
        overflow.width(), overflow.height(), HorizontalTopBottom, LeftToRight);
  }
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

void NGBox::positionUpdated(const NGFragment& fragment) {
  m_layoutBox->setLogicalLeft(fragment.inlineOffset());
  m_layoutBox->setLogicalTop(fragment.blockOffset());
}

bool NGBox::canUseNewLayout() {
  if (!m_layoutBox)
    return true;
  if (m_layoutBox->isLayoutBlockFlow() && !m_layoutBox->childrenInline())
    return true;
  return false;
}
}  // namespace blink
