// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/layout_ng_block_flow.h"

#include "core/layout/LayoutAnalyzer.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment.h"

namespace blink {

LayoutNGBlockFlow::LayoutNGBlockFlow(Element* element)
    : LayoutBlockFlow(element) {}

bool LayoutNGBlockFlow::isOfType(LayoutObjectType type) const {
  return type == LayoutObjectNGBlockFlow || LayoutBlockFlow::isOfType(type);
}

void LayoutNGBlockFlow::layoutBlock(bool relayoutChildren) {
  LayoutAnalyzer::BlockScope analyzer(*this);

  RefPtr<NGConstraintSpace> constraint_space =
      NGConstraintSpace::CreateFromLayoutObject(*this);

  // TODO(layout-dev): This should be created in the constructor once instead.
  // There is some internal state which needs to be cleared between layout
  // passes (probably FirstChild(), etc).
  m_box = new NGBlockNode(this);

  RefPtr<NGLayoutResult> result = m_box->Layout(constraint_space.get());

  if (isOutOfFlowPositioned()) {
    // In legacy layout, abspos differs from regular blocks in that abspos
    // blocks position themselves in their own layout, instead of getting
    // positioned by their parent. So it we are a positioned block in a legacy-
    // layout containing block, we have to emulate this positioning.
    // Additionally, until we natively support abspos in LayoutNG, this code
    // will also be reached though the layoutPositionedObjects call in
    // NGBlockNode::CopyFragmentDataToLayoutBox.
    LogicalExtentComputedValues computedValues;
    computeLogicalWidth(computedValues);
    setLogicalLeft(computedValues.m_position);
    computeLogicalHeight(logicalHeight(), logicalTop(), computedValues);
    setLogicalTop(computedValues.m_position);
  }

  for (auto& descendant : result->OutOfFlowDescendants())
    descendant->UseOldOutOfFlowPositioning();
  clearNeedsLayout();
}

}  // namespace blink
