// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_derived_constraint_space.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_box_iterator.h"
#include "core/layout/ng/ng_fragment.h"
#include "core/layout/LayoutAnalyzer.h"

namespace blink {

LayoutNGBlockFlow::LayoutNGBlockFlow(Element* element)
    : LayoutBlockFlow(element) {}

bool LayoutNGBlockFlow::isOfType(LayoutObjectType type) const {
  return type == LayoutObjectNGBlockFlow || LayoutBlockFlow::isOfType(type);
}

void LayoutNGBlockFlow::layoutBlock(bool relayoutChildren) {
  LayoutAnalyzer::BlockScope analyzer(*this);

  const auto* constraintSpace =
      NGDerivedConstraintSpace::CreateFromLayoutObject(*this);
  NGBox box(this);
  box.layout(constraintSpace);
  clearNeedsLayout();
}

}  // namespace blink
