// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_base_layout_algorithm_test.h"

#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_layout_algorithm.h"
#include "core/layout/ng/ng_layout_result.h"
#include "core/layout/ng/ng_physical_fragment.h"

namespace blink {

NGBaseLayoutAlgorithmTest::NGBaseLayoutAlgorithmTest() {
  RuntimeEnabledFeatures::SetLayoutNGEnabled(true);
}

NGBaseLayoutAlgorithmTest::~NGBaseLayoutAlgorithmTest() {
  RuntimeEnabledFeatures::SetLayoutNGEnabled(false);
}

void NGBaseLayoutAlgorithmTest::SetUp() {
  RenderingTest::SetUp();
  EnableCompositing();
}

std::pair<RefPtr<NGPhysicalBoxFragment>, RefPtr<NGConstraintSpace>>
NGBaseLayoutAlgorithmTest::RunBlockLayoutAlgorithmForElement(Element* element) {
  LayoutNGBlockFlow* block_flow =
      ToLayoutNGBlockFlow(element->GetLayoutObject());
  NGBlockNode node(block_flow);
  RefPtr<NGConstraintSpace> space =
      NGConstraintSpace::CreateFromLayoutObject(*block_flow);

  RefPtr<NGLayoutResult> result = NGBlockLayoutAlgorithm(node, *space).Layout();
  return std::make_pair(
      ToNGPhysicalBoxFragment(result->PhysicalFragment().Get()),
      std::move(space));
}

}  // namespace blink
